/*
 * This file is part of Chiaki.
 *
 * Chiaki is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chiaki is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Chiaki.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <chiaki/congestioncontrol.h>


#define CONGESTION_CONTROL_INTERVAL 200 // ms


static void *congestion_control_thread_func(void *user)
{
	ChiakiCongestionControl *control = user;

	ChiakiErrorCode err = chiaki_mutex_lock(&control->stop_cond_mutex);
	if(err != CHIAKI_ERR_SUCCESS)
		return NULL;

	while(true)
	{
		err = chiaki_cond_timedwait(&control->stop_cond, &control->stop_cond_mutex, CONGESTION_CONTROL_INTERVAL);
		if(err != CHIAKI_ERR_SUCCESS && err != CHIAKI_ERR_TIMEOUT)
			break;
		if(control->stop_cond_predicate)
			break;
		// TODO: detect non-stop and non-timeout (spurious) wakeup and wait the rest of the timeout

		CHIAKI_LOGD(control->takion->log, "Sending Congestion Control Packet\n");
		ChiakiTakionCongestionPacket packet = { 0 }; // TODO: fill with real values
		chiaki_takion_send_congestion(control->takion, &packet);
	}

	chiaki_mutex_unlock(&control->stop_cond_mutex);
	return NULL;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_congestion_control_start(ChiakiCongestionControl *control, ChiakiTakion *takion)
{
	control->takion = takion;

	control->stop_cond_predicate = false;
	ChiakiErrorCode err = chiaki_cond_init(&control->stop_cond);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	err = chiaki_mutex_init(&control->stop_cond_mutex);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		chiaki_cond_fini(&control->stop_cond);
		return err;
	}

	err = chiaki_thread_create(&control->thread, congestion_control_thread_func, control);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		chiaki_mutex_fini(&control->stop_cond_mutex);
		chiaki_cond_fini(&control->stop_cond);
		return err;
	}

	return CHIAKI_ERR_SUCCESS;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_congestion_control_stop(ChiakiCongestionControl *control)
{
	ChiakiErrorCode err = chiaki_mutex_lock(&control->stop_cond_mutex);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	control->stop_cond_predicate = true;
	err = chiaki_cond_signal(&control->stop_cond);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	err = chiaki_mutex_unlock(&control->stop_cond_mutex);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;

	err = chiaki_thread_join(&control->thread, NULL);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;

	chiaki_mutex_fini(&control->stop_cond_mutex);
	chiaki_cond_fini(&control->stop_cond);

	return CHIAKI_ERR_SUCCESS;
}
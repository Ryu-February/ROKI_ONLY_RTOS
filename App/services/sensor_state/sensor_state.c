/*
 * sensor_state.c
 *
 *  Created on: 2026. 6. 15.
 *      Author: RCY
 */

#include "sensor_state.h"


static volatile sensor_snapshot_t s_state;




bool sensor_state_update_ir(bool d)
{
	bool changed;

	changed = (s_state.ir_detected != d);
	s_state.ir_detected = d;
	s_state.seq++;

	return changed;
}

bool sensor_state_update_bat(bool low, uint16_t pct)
{
	bool changed;

	changed = (s_state.vbat_low != low);

	s_state.vbat_low = low;
	s_state.vbat_pct = pct;
	s_state.seq++;

	return changed;
}

bool sensor_state_update_color(color_t color)
{
	bool changed;

	changed = (s_state.color != color);

	s_state.color = color;
	s_state.seq++;

	return changed;
}

bool sensor_state_update_card(color_mode_t card, uint8_t count)
{
	bool changed;

	changed = (s_state.card != card) || (s_state.card_count != count);

	s_state.card = card;
	s_state.card_count = count;
	s_state.seq++;

	return changed;
}

void sensor_state_get(sensor_snapshot_t *out)
{
	*out = s_state;
}

bool sensor_state_get_ir(void)
{
	return s_state.ir_detected;
}

bool sensor_state_get_vbat_low(void)
{
	return s_state.vbat_low;
}

bool sensor_state_get_vbat_pct(void)
{
	return s_state.vbat_pct;
}

color_t sensor_state_get_color(void)
{
	return s_state.color;
}

color_mode_t sensor_state_get_card(void)
{
	return s_state.card;
}

uint8_t sensor_state_get_card_count(void)
{
	return s_state.card_count;
}

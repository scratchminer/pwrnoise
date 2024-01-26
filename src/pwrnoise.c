#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <SDL.h>
#include <blip_buf.h>
#include <tinywav.h>

static blip_t *blip_left;
static blip_t *blip_right;
static bool outFile;

typedef struct {
	bool enable;
	bool am;
	
	uint16_t period;
	uint16_t period_counter;
	
	uint8_t octave;
	uint16_t octave_counter;
	
	uint8_t tapa;
	uint8_t tapb;
	bool tapb_enable;
	
	uint16_t lfsr;
	uint8_t vol;
	
	uint8_t out_latch;
	uint8_t prev;
} noise_channel_t;

void pwrnoise_noise_write(noise_channel_t *chan, uint8_t reg, uint8_t val) {
	switch (reg & 0x1f) {
		case 1:
			chan->enable = (val & 0x80) != 0;
			chan->am = (val & 0x02) != 0;
			chan->tapb_enable = (val & 0x01) != 0;
			break;
		case 2:
			chan->period = (chan->period & 0xf00) | val;
			break;
		case 3:
			chan->period = (chan->period & 0xff) | ((uint16_t)val << 8) & 0xf00;
			chan->octave = val >> 4;
			break;
		case 4:
			chan->lfsr = (chan->lfsr & 0xff00) | val;
			break;
		case 5:
			chan->lfsr = (chan->lfsr & 0x00ff) | ((uint16_t)val << 8);
			break;
		case 6:
			chan->tapa = val >> 4;
			chan->tapb = val & 0x0f;
			break;
		case 7:
			chan->vol = val;
			break;
		default: break;
	}
}

void pwrnoise_noise_step(noise_channel_t *chan, uint16_t cycles) {
	if (!chan->enable) {
		chan->out_latch = 0;
		return;
	}
	
	chan->octave_counter += cycles;
	if (((cycles >= 2) && ((cycles >> (chan->octave + 1)) != 0)) || (!(((chan->octave_counter - 1) >> chan->octave) & 0x0001) && ((chan->octave_counter >> chan->octave) & 0x0001))) {
		chan->period_counter += (cycles >> (chan->octave + 1));
		if ((cycles >> (chan->octave + 1)) == 0) ++chan->period_counter;
		
		while (chan->period_counter >= 4096) {
			chan->prev = (uint8_t)(chan->lfsr >> 15);
			uint16_t in = ((chan->lfsr >> chan->tapa) ^ (chan->tapb_enable ? (chan->lfsr >> chan->tapb) : 0)) & 0x0001;
			chan->lfsr = (chan->lfsr << 1) | in;
			chan->period_counter -= 4096 - chan->period;
		}
	}
	
	chan->out_latch = chan->prev ? chan->vol : 0;
}

typedef struct {
	bool enable;
	uint8_t flags;
	
	uint16_t period;
	uint16_t period_counter;
	
	uint8_t octave;
	uint16_t octave_counter;
	
	uint8_t alength;
	uint8_t blength;
	uint16_t a;
	uint16_t b;
	bool portion;
	
	uint8_t aoffset;
	uint8_t boffset;
	
	uint8_t accum;
	uint8_t vol;
	
	uint8_t out_latch;
	uint8_t prev;
} slope_channel_t;

void pwrnoise_slope_write(slope_channel_t *chan, uint8_t reg, uint8_t val) {
	switch (reg & 0x1f) {
		case 0:
			chan->accum = val & 0x7f;
			break;
		case 1:
			chan->enable = (val & 0x80) != 0;
			if ((val & 0x40) != 0) {
				chan->a = 0;
				chan->b = 0;
				chan->portion = false;
			}
			chan->flags = val & 0x3f;
			break;
		case 2:
			chan->period = (chan->period & 0xf00) | val;
			break;
		case 3:
			chan->period = (chan->period & 0xff) | ((uint16_t)val << 8) & 0xf00;
			chan->octave = val >> 4;
			break;
		case 4:
			chan->alength = val;
			break;
		case 5:
			chan->blength = val;
			break;
		case 6:
			chan->aoffset = val >> 4;
			chan->boffset = val & 0x0f;
			break;
		case 7:
			chan->vol = val;
			break;
		default: break;
	}
}

void pwrnoise_slope_step(slope_channel_t *chan, uint16_t cycles, bool force_zero) {
	if (!chan->enable) {
		chan->out_latch = 0;
		return;
	}
	
	chan->octave_counter += cycles;
	if (((cycles >= 2) && ((cycles >> (chan->octave + 1)) != 0)) || (!(((chan->octave_counter - 1) >> chan->octave) & 0x0001) && ((chan->octave_counter >> chan->octave) & 0x0001))) {
		chan->period_counter += (cycles >> (chan->octave + 1));
		if ((cycles >> (chan->octave + 1)) == 0) ++chan->period_counter;
		
		while (chan->period_counter >= 4096) {
			if (!chan->portion) {
				if ((chan->flags & 0x02) != 0) chan->accum -= chan->aoffset;
				else chan->accum += chan->aoffset;
				
				if ((chan->flags & 0x20) != 0 && chan->accum > 0x7f) chan->accum = (chan->flags & 0x02) ? 0x00 : 0x7f;
				chan->accum &= 0x7f;
				
				if (++chan->a > chan->alength) {
					if ((chan->flags & 0x04) != 0) chan->accum = (chan->flags & 0x01) ? 0x7f : 0x00;
					chan->b = 0x00;
					chan->portion = true;
				}
			}
			else {
				if ((chan->flags & 0x01) != 0) chan->accum -= chan->boffset;
				else chan->accum += chan->boffset;
				
				if ((chan->flags & 0x10) != 0 && chan->accum > 0x7f) chan->accum = (chan->flags & 0x01) ? 0x00 : 0x7f;
				chan->accum &= 0x7f;
				
				if (++chan->b > chan->blength) {
					if ((chan->flags & 0x08) != 0) chan->accum = (chan->flags & 0x02) ? 0x7f : 0x00;
					chan->a = 0x00;
					chan->portion = false;
				}
			}
			
			chan->period_counter -= 4096 - chan->period;
			
			uint8_t left = chan->accum >> 3;
			uint8_t right = chan->accum >> 3;
	
			switch (chan->vol >> 4) {
				case 0:
				case 1:
					left >>= 1;
				case 2:
				case 3:
					left >>= 1;
				case 4:
				case 5:
				case 6:
				case 7:
					left >>= 1;
				default: break;
			}
			switch (chan->vol & 0xf) {
				case 0:
				case 1:
					right >>= 1;
				case 2:
				case 3:
					right >>= 1;
				case 4:
				case 5:
				case 6:
				case 7:
					right >>= 1;
				default: break;
			}
	
			left &= (chan->vol >> 4);
			right &= (chan->vol & 0xf);
			chan->prev = (left << 4) | right;
		}
	}
	
	chan->out_latch = force_zero ? 0 : chan->prev;
}

typedef struct {
	uint8_t flags;
	uint8_t gpioa;
	uint8_t gpiob;
	
	noise_channel_t n1;
	noise_channel_t n2;
	noise_channel_t n3;
	slope_channel_t s;
} power_noise_t;

void pwrnoise_write(power_noise_t *pn, uint8_t reg, uint8_t val) {
	reg &= 0x1f;
	
	if (reg == 0x00) {
		pn->flags = val;
	}
	else if (reg == 0x08 && !(pn->flags & 0x20)) {
		pn->gpioa = val;
	}
	else if (reg == 0x10 && !(pn->flags & 0x40)) {
		pn->gpiob = val;
	}
	else if (reg < 0x08) {
		pwrnoise_noise_write(&pn->n1, reg % 8, val);
	}
	else if (reg < 0x10) {
		pwrnoise_noise_write(&pn->n2, reg % 8, val);
	}
	else if (reg < 0x18) {
		pwrnoise_noise_write(&pn->n3, reg % 8, val);
	}
	else {
		pwrnoise_slope_write(&pn->s, reg % 8, val);
	}
}

void pwrnoise_step(power_noise_t *pn, uint16_t cycles, int16_t *left, int16_t *right) {
	int32_t final_left, final_right;
	
	if ((pn->flags & 0x80) != 0) {
		pwrnoise_noise_step(&pn->n1, cycles);
		pwrnoise_noise_step(&pn->n2, cycles);
		pwrnoise_noise_step(&pn->n3, cycles);
		pwrnoise_slope_step(&pn->s, cycles, (pn->n1.am && !(pn->n1.prev)) || (pn->n2.am && !(pn->n2.prev)) || (pn->n3.am && !(pn->n3.prev)));
		
		final_left = (pn->n1.out_latch >> 4) + (pn->n2.out_latch >> 4) + (pn->n3.out_latch >> 4) + (pn->s.out_latch >> 4);
		final_right = (pn->n1.out_latch & 0xf) + (pn->n2.out_latch & 0xf) + (pn->n3.out_latch & 0xf) + (pn->s.out_latch & 0xf);
	}
	else {
		final_left = 0;
		final_right = 0;
	}
	
	*left = (int16_t)((final_left * 65535 / 63 - 32768) * (pn->flags & 0x7) / 7);
	*right = (int16_t)((final_right * 65535 / 63 - 32768) * (pn->flags & 0x7) / 7);
}

int main(int argc, const char **argv) {
	if (argc < 2) {
		printf("Usage: pwrnoise (file name) [output WAV file name]\n");
		return 1;
	}
	
	const char *filename = argv[1];
	FILE *fh = fopen(filename, "r");
	
	if (fh == NULL) {
		printf("Error: the file could not be opened. Does it exist?\n");
		return 1;
	}
	
	char magic[8];
	int size = fread(magic, 1, 8, fh);
	if (size != 8) {
		printf("Error: encountered EOF while reading signature\n");
		return 1;
	}
	if(strncmp(magic, "PWRNOISE", 8)) {
		printf("Error: encountered incorrect signature (should be 'PWRNOISE')\n");
		return 1;
	}
	
	uint32_t clock_rate;
	size = fread(&clock_rate, 4, 1, fh);
	if (size != 1) {
		printf("Error: encountered EOF while reading clock rate\n");
		return 1;
	}
	
	if(SDL_Init(SDL_INIT_AUDIO)) {
		printf("Error: unable to initialize SDL");
		return 1;
	}
	
	blip_left = blip_new(4096);
	blip_right = blip_new(4096);
	blip_set_rates(blip_left, clock_rate, 44100);
	blip_set_rates(blip_right, clock_rate, 44100);
	
	power_noise_t pn;
	memset(&pn, 0, sizeof(pn));
	
	uint32_t sleep_cyc = 0;
	
	int16_t prev_left = 0;
	int16_t prev_right = 0;
	
	TinyWav out;
	SDL_AudioDeviceID dev;
	
	if(argc > 2) {
		if (tinywav_open_write(&out, 2, 44100, TW_INT16, TW_INTERLEAVED, argv[2])) {
			printf("Warning: output file could not be opened, playing track instead\n");
			outFile = false;
		}
		else {
			outFile = true;
		}
	}
	else {
		SDL_AudioSpec want;
		SDL_zero(want);
		want.freq = 44100;
		want.format = AUDIO_S16LSB;
		want.channels = 2;
		want.samples = 512;
		
		dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
		SDL_PauseAudioDevice(dev, 0);
		outFile = false;
	}
	
	short aud_data[1024];
	
	while (1) {
		for (int cyc = 0; cyc < 4096; cyc++) {
			if (sleep_cyc > 0) sleep_cyc--;
			else {
				uint8_t reg, val;
				size = fread(&reg, 1, 1, fh);
				if (size != 1) {
					if (outFile) {
						tinywav_close_write(&out);
					}
					else {
						do {} while(SDL_GetQueuedAudioSize(dev) > 0);
						SDL_PauseAudioDevice(dev, 1);
						SDL_CloseAudioDevice(dev);
						SDL_Quit();
					}
					blip_delete(blip_left);
					blip_delete(blip_right);
					fclose(fh);
					return 0;
				}
				
				if (reg == 0xff) {
					size = fread(&sleep_cyc, 3, 1, fh);
					if (size != 1) {
						if (outFile) {
							tinywav_close_write(&out);
						}
						else {
							do {} while(SDL_GetQueuedAudioSize(dev) > 0);
							SDL_PauseAudioDevice(dev, 1);
							SDL_CloseAudioDevice(dev);
							SDL_Quit();
						}
						blip_delete(blip_left);
						blip_delete(blip_right);
						fclose(fh);
						return 0;
					}
					sleep_cyc &= 0xffffff;
				}
				else {
					size = fread(&val, 1, 1, fh);
					if (size != 1) {
						if (outFile) {
							tinywav_close_write(&out);
						}
						else {
							do {} while(SDL_GetQueuedAudioSize(dev) > 0);
							SDL_PauseAudioDevice(dev, 1);
							SDL_CloseAudioDevice(dev);
							SDL_Quit();
						}
						blip_delete(blip_left);
						blip_delete(blip_right);
						fclose(fh);
						return 0;
					}
					pwrnoise_write(&pn, reg, val);
				}
			}
			
			int16_t left, right;
			pwrnoise_step(&pn, 1, &left, &right);
			
			blip_add_delta_fast(blip_left, cyc, left - prev_left);
			prev_left = left;
			blip_add_delta_fast(blip_right, cyc, right - prev_right);
			prev_right = right;
		}
		
		blip_end_frame(blip_left, 4095);
		blip_end_frame(blip_right, 4095);
		
		int lsz = blip_read_samples(blip_left, aud_data, 512, 1);
		int rsz = blip_read_samples(blip_right, aud_data + 1, 512, 1);
		int sz = ((lsz > rsz) ? lsz : rsz);
		
		if (outFile) tinywav_write_f(&out, aud_data, sz);
		else SDL_QueueAudio(dev, aud_data, sz * 2 * sizeof(short));
	}
}

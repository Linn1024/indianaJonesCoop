/* Indiana co-op additions, GPL-2.0-or-later. */
#ifndef INDIANA_COOP_H
#define INDIANA_COOP_H
#include <stdint.h>
#ifdef _WIN32
#define IJ_API __declspec(dllexport)
#else
#define IJ_API __attribute__((visibility("default")))
#endif
extern int ij_loaded;
extern int ij_extra_cycles;
int ij_extra_work(uint16_t pc);
void ij_prepare_load(void);
void ij_finish_load(void);
IJ_API unsigned ij_noclip(void);
IJ_API unsigned ij_death_mode(void);
IJ_API unsigned ij_level_choice(void);
IJ_API void ij_level_current(void);
IJ_API unsigned ij_player_out(unsigned player);
void ij_init(void);
void ij_close(void);
uint16_t ij_instruction(uint16_t pc);
IJ_API void ij_enable(unsigned enabled);
IJ_API unsigned ij_read(unsigned player, unsigned address);
IJ_API void ij_write(unsigned player, unsigned address, unsigned value);
IJ_API void ij_status(unsigned *out);
IJ_API int ij_cheat(unsigned action);
IJ_API void ij_rejoin(unsigned player);
IJ_API void ij_test_pc(unsigned pc);
IJ_API unsigned ij_graphics_errors(void);
uint32_t ij_menu_input(uint32_t pads);
IJ_API int ij_menu_visible(void);
int ij_start_menu_visible(void);
int ij_transition_hidden(void);
void ij_menu_draw(uint8_t *pixels);
const uint32_t *ij_video(const uint32_t *pixels,unsigned width,unsigned height,unsigned stride,unsigned crop_left,unsigned crop_top);
#endif

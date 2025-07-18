// #include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debugger.h"
#include "emulator.h"
#include "nds.h"
#include "types.h"

#include <js/glue.h>

char wintitle[200];
uint16_t pixels[(NDS_SCREEN_H*2) * NDS_SCREEN_W];
uint32_t dest[(NDS_SCREEN_H*2) * NDS_SCREEN_W];
void __unordtf2() {}
void rgb555_to_rgba() {
    for (size_t i = 0; i < (NDS_SCREEN_H * 2) * NDS_SCREEN_W; i++) {
        uint8_t b5 = (pixels[i] >> 10) & 0x1F;
        uint8_t g5 = (pixels[i] >> 5) & 0x1F;
        uint8_t r5 = pixels[i] & 0x1F;

        uint8_t r8 = (r5 << 3) | (r5 >> 2);
        uint8_t g8 = (g5 << 3) | (g5 >> 2);
        uint8_t b8 = (b5 << 3) | (b5 >> 2);

        dest[i] = (255 << 24) | (b8 << 16) | (g8 << 8) | r8;
    }
}

uint8_t keys[UINT16_MAX] = {0};
static bool onkeydown(void *user_data, int key, int code, int modifiers) {
    hotkey_press(key, code);
    keys[code] = 1;
    return 1;
}

static bool onkeyup(void *user_data, int key, int code, int modifiers) {
    keys[code] = 0;
    return 1;
}

// static inline void center_screen_in_window(int windowW, int windowH,
//                                            SDL_Rect* dst) {
//     if (windowW * (2 * NDS_SCREEN_H) / NDS_SCREEN_W > windowH) {
//         dst->h = windowH;
//         dst->y = 0;
//         dst->w = dst->h * NDS_SCREEN_W / (2 * NDS_SCREEN_H);
//         dst->x = (windowW - dst->w) / 2;
//     } else {
//         dst->w = windowW;
//         dst->x = 0;
//         dst->h = dst->w * (2 * NDS_SCREEN_H) / NDS_SCREEN_W;
//         dst->y = (windowH - dst->h) / 2;
//     }
// }

int main(int argc, char** argv) {

    JS_createCanvas(NDS_SCREEN_W, 2 * NDS_SCREEN_H);
    if (emulator_init(argc, argv) < 0) return -1;
    JS_addKeyDownEventListener(NULL, onkeydown);
    JS_addKeyUpEventListener(NULL, onkeyup);

    JS_addMouseMoveEventListener(NULL, onmousemove);
    JS_addMouseDownEventListener(NULL, onmousedown);
    JS_addMouseUpEventListener(NULL, onmouseup);


    // init_gpu_thread(&ntremu.nds->gpu);

    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    // SDL_GameController* controller = NULL;
    // if (SDL_NumJoysticks() > 0) {
    //     controller = SDL_GameControllerOpen(0);
    // }

    // SDL_Window* window;
    // SDL_Renderer* renderer;
    // SDL_CreateWindowAndRenderer(NDS_SCREEN_W * 2, NDS_SCREEN_H * 4,
    //                             SDL_WINDOW_RESIZABLE, &window, &renderer);
    snprintf(wintitle, 199, "ntremu | %s | %.2lf FPS", ntremu.romfilenodir,
             0.0);
    // SDL_SetWindowTitle(window, wintitle);
    // SDL_RenderClear(renderer);
    // SDL_RenderPresent(renderer);

    // SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555,
    //                                          SDL_TEXTUREACCESS_STREAMING,
    //                                          NDS_SCREEN_W, 2 * NDS_SCREEN_H);

    // SDL_AudioSpec audio_spec = {.freq = SAMPLE_FREQ,
    //                             .format = AUDIO_F32,
    //                             .channels = 2,
    //                             .samples = SAMPLE_BUF_LEN / 2};
    // SDL_AudioDeviceID audio =
    //     SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    // SDL_PauseAudioDevice(audio, 0);

    uint64_t prev_time = JS_performanceNow();
    uint64_t prev_fps_update = prev_time;
    uint64_t prev_fps_frame = 0;
    const uint64_t frame_ticks = 1000 / 60;
    uint64_t frame = 0;

    bool bkpthit = false;

    ntremu.running = !ntremu.debugger;
    while (true) {
        while (ntremu.running) {
            uint64_t cur_time;
            uint64_t elapsed;

            bkpthit = false;

            // bool play_audio = !(ntremu.pause || ntremu.mute || ntremu.uncap);

            if (!(ntremu.pause)) {
                do {
                    while (!ntremu.nds->frame_complete) {
                        if (ntremu.debugger) {
                            if (ntremu.nds->cur_cpu->cur_instr_addr ==
                                ntremu.breakpoint) {
                                bkpthit = true;
                                break;
                            }
                            nds_step(ntremu.nds);
                        } else {
                            nds_run(ntremu.nds);
                        }
                        if (ntremu.nds->cpuerr) break;
                        if (ntremu.nds->samples_full) {
                            ntremu.nds->samples_full = false;
                        //  if (play_audio) {
                        //      SDL_QueueAudio(audio,
                        //                     ntremu.nds->spu.sample_buf,
                        //                     SAMPLE_BUF_LEN * 4);
                        //  }
                        }
                    }
                    if (bkpthit || ntremu.nds->cpuerr) break;
                    ntremu.nds->frame_complete = false;
                    frame++;

                 cur_time = JS_performanceNow();
                 elapsed = cur_time - prev_time;
                } while (ntremu.uncap && elapsed < frame_ticks);
            }
            if (bkpthit || ntremu.nds->cpuerr) break;

            memcpy(pixels, ntremu.nds->screen_top,
                   sizeof ntremu.nds->screen_top);
            memcpy(pixels + (sizeof ntremu.nds->screen_top / sizeof(uint16_t)),
                   ntremu.nds->screen_bottom, sizeof ntremu.nds->screen_bottom);

            SDL_Rect dst = {0, 0, NDS_SCREEN_W, NDS_SCREEN_H * 2};
            rgb555_to_rgba();
            JS_setPixelsAlpha(dest);
            JS_requestAnimationFrame();
            // JS_setTimeout(1);

            if (ntremu.freecam) {
                update_input_freecam(keys);
            } else {
                update_input_keyboard(ntremu.nds, keys);
            }
            // if (controller) update_input_controller(ntremu.nds, controller);
            dst.h /= 2;
            dst.y += dst.h;
            update_input_touch(ntremu.nds, &dst);

            if (!ntremu.uncap) {
            //     if (play_audio) {
            //         while (SDL_GetQueuedAudioSize(audio) >= 16 * SAMPLE_BUF_LEN)
                        // JS_setTimeout(1);
            //     } else {
                    cur_time = JS_performanceNow();
                    elapsed = cur_time - prev_time;
                    int64_t wait = frame_ticks - elapsed;
                    int64_t waitMS =
                        wait * 1000 / (int64_t) 1000;
                    if (waitMS > 1 && !ntremu.uncap) {
                        JS_setTimeout(waitMS);
                    }
            //     }
            }
            cur_time = JS_performanceNow();
            elapsed = cur_time - prev_fps_update;
            if (elapsed >= 500) { // 1000 ms / 2
                double fps = (double) 1000 *
                             (frame - prev_fps_frame) / elapsed;
                snprintf(wintitle, 199, "ntremu | %s | %.2lf FPS",
                         ntremu.romfilenodir, fps);
                JS_setTitle(wintitle);
                prev_fps_update = cur_time;
                prev_fps_frame = frame;
            }
            prev_time = cur_time;

            if (ntremu.frame_adv) {
                ntremu.running = false;
                ntremu.frame_adv = false;
            }
        }

        if (ntremu.debugger) {
            ntremu.running = false;
            if (bkpthit) {
                printf("Breakpoint hit: %08x\n", ntremu.breakpoint);
            }
            debugger_run();
        } else {
            break;
        }
    }

#ifdef CPULOG
    FILE* fp = fopen("arm7.log", "w");
    for (int i = 0; i < LOGMAX; i++) {
        u32 li = (ntremu.nds->cpu7.c.log_idx + i) % LOGMAX;
        fprintf(fp, "%08x: ", ntremu.nds->cpu7.c.log[li].addr);
        arm_disassemble(ntremu.nds->cpu7.c.log[li].instr,
                        ntremu.nds->cpu7.c.log[li].addr, fp);
        fprintf(fp, "\n");
    }
    fclose(fp);
    fp = fopen("arm9.log", "w");
    for (int i = 0; i < LOGMAX; i++) {
        u32 li = (ntremu.nds->cpu9.c.log_idx + i) % LOGMAX;
        fprintf(fp, "%08x: ", ntremu.nds->cpu9.c.log[li].addr);
        arm_disassemble(ntremu.nds->cpu9.c.log[li].instr,
                        ntremu.nds->cpu9.c.log[li].addr, fp);
        fprintf(fp, "\n");
    }
    fclose(fp);

    fp = fopen("ram.bin", "wb");
    fwrite(ntremu.nds->ram, RAMSIZE, 1, fp);
    fclose(fp);
#endif

    // if (controller) SDL_GameControllerClose(controller);

    // SDL_CloseAudioDevice(audio);

    // SDL_DestroyRenderer(renderer);
    // SDL_DestroyWindow(window);

    // SDL_Quit();

    // destroy_gpu_thread();

    emulator_quit();

    return 0;
}

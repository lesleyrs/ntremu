#include "emulator.h"

// #include <SDL2/SDL.h>
// #include <fcntl.h>
#include <js/glue.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <unistd.h>

#include "arm/arm.h"
#include "emulator_state.h"
// NOTE: from https://github.com/melonDS-emu/melonDS/blob/master/src/FreeBIOS.cpp
#include "freebios.h"
#include "nds.h"
#include "arm/thumb.h"

#define TRANSLATE_SPEED 5.0
#define ROTATE_SPEED 0.02

EmulatorState ntremu;

const char usage[] = "ntremu [options] <romfile>\n"
                     "-b -- boot from firmware\n"
                     "-d -- run the debugger\n"
                     "-p <path> -- path to bios/firmware files\n"
                     "-s <path> -- path to SD card image for DLDI\n"
                     "-h -- print help";

int emulator_init(int argc, char** argv) {
    read_args(argc, argv);
    if (!ntremu.romfile) {
        eprintf(usage);
#ifndef __wasm
        return -1;
#endif
    }

    FILE *f = NULL;
    if (ntremu.biosPath) {
        char buf[UINT8_MAX];
        // sprintf(buf, "%s/drastic_bios_arm7.bin", ntremu.biosPath);
        sprintf(buf, "%s/bios7.bin", ntremu.biosPath);
        FILE *bios7 = fopen(buf, "rb");
        // sprintf(buf, "%s/drastic_bios_arm9.bin", ntremu.biosPath);
        sprintf(buf, "%s/bios9.bin", ntremu.biosPath);
        FILE *bios9 = fopen(buf, "rb");
        if (bios7 && bios9) {
            ntremu.bios7 = malloc(BIOS7SIZE);
            fread(ntremu.bios7, 1, BIOS7SIZE, bios7);
            ntremu.bios9 = malloc(BIOS9SIZE);
            fread(ntremu.bios9, 1, BIOS9SIZE, bios9);
        }
    } else {
        printf("No bios found, using drastic bios fallback\n");
        ntremu.bios7 = malloc(BIOS7SIZE);
        memcpy(ntremu.bios7, bios_arm7_bin, BIOS7SIZE);

        ntremu.bios9 = malloc(BIOS9SIZE);
        memcpy(ntremu.bios9, bios_arm9_bin, BIOS9SIZE);
    }

    ntremu.firmware = malloc(FIRMWARESIZE);
    FILE *firmware = fopen("firmware.bin", "rb");
    if (!firmware) {
        eprintf("Missing firmware.bin, some things might not work (touch input)");
    } else {
        fread(ntremu.firmware, 1, FIRMWARESIZE, firmware);
        fclose(firmware);
    }
    ntremu.nds = malloc(sizeof *ntremu.nds);
    ntremu.card = create_card(ntremu.romfile);
    if (!ntremu.card) {
        ntremu.card = create_card_from_picker(&ntremu.romfile);
        if (!ntremu.card) {
            eprintf("Invalid rom file\n");
            return -1;
        }
    }

    if (ntremu.sd_path) {
        // ntremu.dldi_sd_fd = open(ntremu.sd_path, O_RDWR);
        // if (ntremu.dldi_sd_fd >= 0) {
        //     struct stat st;
        //     fstat(ntremu.dldi_sd_fd, &st);
        //     if (S_ISBLK(st.st_mode)) {
        //         ntremu.dldi_sd_size = lseek(ntremu.dldi_sd_fd, 0, SEEK_END);
        //     } else {
        //         ntremu.dldi_sd_size = st.st_size;
        //     }
        // }
    } else {
        ntremu.dldi_sd_fd = -1;
    }

    arm_generate_lookup();
    thumb_generate_lookup();
    generate_adpcm_table();

    emulator_reset();

    ntremu.romfilenodir = strrchr(ntremu.romfile, '/');
    if (ntremu.romfilenodir) ntremu.romfilenodir++;
    else ntremu.romfilenodir = ntremu.romfile;
    return 0;
}

void emulator_quit() {
    // close(ntremu.dldi_sd_fd);
    destroy_card(ntremu.card);
    free(ntremu.nds);
    // munmap(ntremu.bios7, BIOS7SIZE);
    // munmap(ntremu.bios9, BIOS9SIZE);
    // munmap(ntremu.firmware, FIRMWARESIZE);
}

void emulator_reset() {
    init_nds(ntremu.nds, ntremu.card, ntremu.bios7, ntremu.bios9,
             ntremu.firmware, ntremu.bootbios);
}

void read_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (char* f = &argv[i][1]; *f; f++) {
                switch (*f) {
                    case 'd':
                        ntremu.debugger = true;
                        break;
                    case 'b':
                        ntremu.bootbios = true;
                        break;
                    case 'p':
                        if (!f[1] && i + 1 < argc) {
                            ntremu.biosPath = argv[++i];
                        } else {
                            eprintf("Missing argument for '-p'\n");
                        }
                        break;
                    case 's':
                        if (!f[1] && i + 1 < argc) {
                            ntremu.sd_path = argv[++i];
                        } else {
                            eprintf("Missing argument for '-s'\n");
                        }
                        break;
                    case 'h':
                        eprintf(usage);
                        exit(0);
                    default:
                        eprintf("Invalid argument\n");
                }
            }
        } else {
            ntremu.romfile = argv[i];
        }
    }
}

#include <js/dom_pk_codes.h>
void hotkey_press(int key, int code) {
    switch (key) {
        case 'p':
            ntremu.pause = !ntremu.pause;
            break;
        case 'm':
            ntremu.mute = !ntremu.mute;
            break;
        case 'r':
            emulator_reset();
            ntremu.pause = false;
            break;
        case 'o':
            ntremu.wireframe = !ntremu.wireframe;
            break;
        case 'c':
            if (ntremu.freecam) {
                ntremu.freecam = false;
            } else {
                ntremu.freecam = true;
                ntremu.freecam_mtx = (mat4){0};
                ntremu.freecam_mtx.p[0][0] = 1;
                ntremu.freecam_mtx.p[1][1] = 1;
                ntremu.freecam_mtx.p[2][2] = 1;
                ntremu.freecam_mtx.p[3][3] = 1;
            }
            break;
        case 'u':
            ntremu.abs_touch = !ntremu.abs_touch;
            break;
        default:
            break;
    }
    switch (code) {
        case DOM_PK_TAB:
            ntremu.uncap = !ntremu.uncap;
            break;
        case DOM_PK_BACKSPACE:
            if (ntremu.nds->io7.extkeyin.hinge) {
                ntremu.nds->io7.extkeyin.hinge = 0;
                ntremu.nds->io7.ifl.unfold = 1;
            } else {
                ntremu.nds->io7.extkeyin.hinge = 1;
            }
            break;
    }
}

void update_input_keyboard(NDS* nds, uint8_t *keys) {
    nds->io7.keyinput.a = ~keys[DOM_PK_Z];
    nds->io7.keyinput.b = ~keys[DOM_PK_X];
    nds->io7.keyinput.start = ~keys[DOM_PK_ENTER];
    nds->io7.keyinput.select = ~keys[DOM_PK_SHIFT_RIGHT];
    nds->io7.keyinput.left = ~keys[DOM_PK_ARROW_LEFT];
    nds->io7.keyinput.right = ~keys[DOM_PK_ARROW_RIGHT];
    nds->io7.keyinput.up = ~keys[DOM_PK_ARROW_UP];
    nds->io7.keyinput.down = ~keys[DOM_PK_ARROW_DOWN];
    nds->io7.keyinput.l = ~keys[DOM_PK_Q];
    nds->io7.keyinput.r = ~keys[DOM_PK_W];
    nds->io9.keyinput = nds->io7.keyinput;

    nds->io7.extkeyin.x = ~keys[DOM_PK_A];
    nds->io7.extkeyin.y = ~keys[DOM_PK_S];
}

/* void update_input_controller(NDS* nds, SDL_GameController* controller) {
    nds->io7.keyinput.a &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
    nds->io7.keyinput.b &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
    nds->io7.keyinput.start &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
    nds->io7.keyinput.select &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK);
    nds->io7.keyinput.left &= ~SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    nds->io7.keyinput.right &= ~SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    nds->io7.keyinput.up &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
    nds->io7.keyinput.down &= ~SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    nds->io7.keyinput.l &= ~SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    nds->io7.keyinput.r &= ~SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    nds->io9.keyinput = nds->io7.keyinput;

    nds->io7.extkeyin.x &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y);
    nds->io7.extkeyin.y &=
        ~SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
} */

static int mouse_x = 0, mouse_y = 0;
static bool mouse_down = false;
bool onmousemove(void *user_data, int button, int x, int y) {
    mouse_x = x;
    mouse_y = y;
    return 0;
}

bool onmousedown(void *user_data, int button, int x, int y) {
    if (button == BUTTON_LEFT) {
        mouse_down = true;
    }
    return 0;
}

bool onmouseup(void *user_data, int button, int x, int y) {
    if (button == BUTTON_LEFT) {
        mouse_down = false;
    }
    return 0;
}

void update_input_touch(NDS* nds, SDL_Rect* ts_bounds) {
    int x = mouse_x, y = mouse_y;
    bool pressed = mouse_down;
    x = (x - ts_bounds->x) * NDS_SCREEN_W / ts_bounds->w;
    y = (y - ts_bounds->y) * NDS_SCREEN_H / ts_bounds->h;
    if (x < 0 || x >= NDS_SCREEN_W || y < 0 || y >= NDS_SCREEN_H)
        pressed = false;
    if (pressed) {
        nds->tsc.x = x;
        nds->tsc.y = y;
    }

    /* if (controller) {
        int x =
            SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        int y =
            SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (abs(x) >= 4000 || abs(y) >= 4000) {
            pressed = true;

            if (ntremu.abs_touch) {
                nds->tsc.x =
                    NDS_SCREEN_W / 2 + (x * (NDS_SCREEN_W / 2 - 10) >> 15);
                nds->tsc.y =
                    NDS_SCREEN_H / 2 + (y * (NDS_SCREEN_H / 2 - 10) >> 15);
            } else {
                static int prev_x = 0, prev_y = 0;

                x >>= 13, y >>= 13;

                if (nds->tsc.y == (u8) -1) {
                    nds->tsc.x = NDS_SCREEN_W / 2;
                    nds->tsc.y = NDS_SCREEN_H / 2;
                } else if (prev_x != x || prev_y != y) {
                    int tmpx = nds->tsc.x;
                    int tmpy = nds->tsc.y;
                    tmpx += x;
                    tmpy += y;
                    if (tmpx < 0) tmpx = 0;
                    if (tmpx >= NDS_SCREEN_W) tmpx = NDS_SCREEN_W - 1;
                    if (tmpy < 0) tmpy = 0;
                    if (tmpy >= NDS_SCREEN_H) tmpy = NDS_SCREEN_H - 1;

                    nds->tsc.x = tmpx;
                    nds->tsc.y = tmpy;
                }
                prev_x = x, prev_y = y;
            }
        }
    } */

    nds->io7.extkeyin.pen = !pressed;
    if (!pressed) {
        nds->tsc.x = -1;
        nds->tsc.y = -1;
    }
}

void matmul2(mat4* a, mat4* b, mat4* dst) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += a->p[i][k] * b->p[k][j];
            }
            dst->p[i][j] = sum;
        }
    }
}

void update_input_freecam(uint8_t *keys) {
    float speed = TRANSLATE_SPEED;
    if (keys[DOM_PK_SHIFT_LEFT] || keys[DOM_PK_SHIFT_RIGHT]) speed /= 20;

    if (keys[DOM_PK_E]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[1][3] = -speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_Q]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[1][3] = speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_ARROW_DOWN]) {
        mat4 m = {0};
        m.p[3][3] = 1;
        m.p[0][0] = 1;
        m.p[1][1] = cosf(ROTATE_SPEED);
        m.p[1][2] = -sinf(ROTATE_SPEED);
        m.p[2][1] = sinf(ROTATE_SPEED);
        m.p[2][2] = cosf(ROTATE_SPEED);
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_ARROW_UP]) {
        mat4 m = {0};
        m.p[3][3] = 1;
        m.p[0][0] = 1;
        m.p[1][1] = cosf(-ROTATE_SPEED);
        m.p[1][2] = -sinf(-ROTATE_SPEED);
        m.p[2][1] = sinf(-ROTATE_SPEED);
        m.p[2][2] = cosf(-ROTATE_SPEED);
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_A]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[0][3] = speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_D]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[0][3] = -speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_ARROW_LEFT]) {
        mat4 m = {0};
        m.p[3][3] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = cosf(-ROTATE_SPEED);
        m.p[2][0] = -sinf(-ROTATE_SPEED);
        m.p[0][2] = sinf(-ROTATE_SPEED);
        m.p[0][0] = cosf(-ROTATE_SPEED);
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_ARROW_RIGHT]) {
        mat4 m = {0};
        m.p[3][3] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = cosf(ROTATE_SPEED);
        m.p[2][0] = -sinf(ROTATE_SPEED);
        m.p[0][2] = sinf(ROTATE_SPEED);
        m.p[0][0] = cosf(ROTATE_SPEED);
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_W]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[2][3] = speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }
    if (keys[DOM_PK_S]) {
        mat4 m = {0};
        m.p[0][0] = 1;
        m.p[1][1] = 1;
        m.p[2][2] = 1;
        m.p[3][3] = 1;
        m.p[2][3] = -speed;
        mat4 tmp;
        matmul2(&m, &ntremu.freecam_mtx, &tmp);
        ntremu.freecam_mtx = tmp;
    }

    ntremu.nds->io7.keyinput.keys = 0x3ff;
    ntremu.nds->io9.keyinput.keys = 0x3ff;
    ntremu.nds->io7.extkeyin.x = 1;
    ntremu.nds->io7.extkeyin.y = 1;
}

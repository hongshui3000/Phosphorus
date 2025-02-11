#include "includes.h"
#include "GPU_driver.h"
#include "tank_game.h"

#define BOTH_EMPTY (UART_LS_TEMT | UART_LS_THRE)

#define WAIT_FOR_XMITR \
        do { \
                lsr = REG8(UART_BASE + UART_LS_REG); \
        } while ((lsr & BOTH_EMPTY) != BOTH_EMPTY)

#define WAIT_FOR_THRE \
        do { \
                lsr = REG8(UART_BASE + UART_LS_REG); \
        } while ((lsr & UART_LS_THRE) != UART_LS_THRE)

#define TASK_STK_SIZE 1024

OS_STK TaskStartStk[TASK_STK_SIZE];

char Info[103]={0xC9,0xCF,0xB5,0xDB,0xCB,0xB5,0xD2,0xAA,0xD3,0xD0,0xB9,0xE2,0xA3,0xAC,0xD3,0xDA,0xCA,0xC7,0xBE,0xCD,0xD3,0xD0,0xC1,0xCB,0xB9,0xE2,0x0D,0x0A,0xC9,0xCF,0xB5,0xDB,0xCB,0xB5,0xD2,0xAA,0xD3,0xD0,0xCC,0xEC,0xBF,0xD5,0xA3,0xAC,0xD3,0xDA,0xCA,0xC7,0xBE,0xCD,0xD3,0xD0,0xC1,0xCB,0xCC,0xEC,0xBF,0xD5,0x0D,0x0A,0xC9,0xCF,0xB5,0xDB,0xCB,0xB5,0xD2,0xAA,0xD3,0xD0,0xC2,0xBD,0xB5,0xD8,0xBA,0xCD,0xBA,0xA3,0xD1,0xF3,0xA3,0xAC,0xD3,0xDA,0xCA,0xC7,0xBE,0xCD,0xD3,0xD0,0xC1,0xCB,0xC2,0xBD,0xB5,0xD8,0xBA,0xCD,0xBA,0xA3,0xD1,0xF3,0x0D};

void uart_init(void)
{
        INT32U divisor;
 
         /* Set baud rate */
	
        divisor = (INT32U) IN_CLK/(16 * UART_BAUD_RATE);

        REG8(UART_BASE + UART_LC_REG) = 0x80;
        REG8(UART_BASE + UART_DLB1_REG) = divisor & 0x000000ff;
        REG8(UART_BASE + UART_DLB2_REG) = (divisor >> 8) & 0x000000ff;
        REG8(UART_BASE + UART_LC_REG) = 0x00;
        
        
        /* Disable all interrupts */
       
        REG8(UART_BASE + UART_IE_REG) = 0x00;
       
 
        /* Set 8 bit char, 1 stop bit, no parity */
        
       REG8(UART_BASE + UART_LC_REG) = UART_LC_WLEN8 | (UART_LC_ONE_STOP | UART_LC_NO_PARITY);
        
  
        uart_print_str("UART initialize done ! \n");
	return;
}

void uart_putc(char c)
{
        unsigned char lsr;
        WAIT_FOR_THRE;
        REG8(UART_BASE + UART_TH_REG) = c;
        if(c == '\n') {
          WAIT_FOR_THRE;
          REG8(UART_BASE + UART_TH_REG) = '\r';
        }
        WAIT_FOR_XMITR;  
  
}

void uart_print_str(char* str)
{
       INT32U i=0;
       OS_CPU_SR cpu_sr;
       OS_ENTER_CRITICAL()
       
       while(str[i]!=0)
       {
       	uart_putc(str[i]);
        i++;
       }
        
       OS_EXIT_CRITICAL()
        
}

void gpio_init()
{
	REG32(GPIO_BASE + GPIO_OE_REG) = 0xffffffff;
	REG32(GPIO_BASE + GPIO_INTE_REG) = 0x00000000;
	gpio_out(0x0f0f0f0f);
	uart_print_str("GPIO initialize done ! \n");
        return;
}

void gpio_out(INT32U number)
{
	

	  REG32(GPIO_BASE + GPIO_OUT_REG) = number;
	  

}

INT32U gpio_in()
{
	INT32U temp = 0;
	

	
	 temp = REG32(GPIO_BASE + GPIO_IN_REG);
	  

	
	return temp;
}

void OSInitTick(void)
{
    INT32U compare = (INT32U)(IN_CLK / OS_TICKS_PER_SEC);
    
    asm volatile("mtc0   %0,$9"   : :"r"(0x0)); 
    asm volatile("mtc0   %0,$11"   : :"r"(compare));  
    asm volatile("mtc0   %0,$12"   : :"r"(0x10000401));
    //uart_print_str("OSInitTick Done!!!\n");
    
    return; 
}

/* ------------------------- GPU ------------------------------- */
void gpu_start_render(void) {
    REG32(GPU_BASE + GPU_RENDER_REG) = 1;
    REG32(GPU_BASE + GPU_OUTPUT_REG) = 1;
}

void gpu_stop_render(void) {
    REG32(GPU_BASE + GPU_RENDER_REG) = 0;
}

// void debug_convert_to_hex(INT32U value, char* buffer) {
//     int i = 7;
//     for (; i >= 0; --i) {
//         int hex_byte = value % 16;
//         value = value >> 4;

//         if (hex_byte >= 10) {
//             buffer[i] = 'a' + hex_byte - 10;
//         }
//         else {
//             buffer[i] = '0' + hex_byte;
//         }
//     }
// }


void gpu_copy_to_vram(unsigned int dst_base_offset, INT32U* src_mem_ptr, unsigned int mem_size) {
    int i = 0;

    // char hex_str[10];
    // hex_str[8] = '\n';
    // hex_str[9] = '\0';

    for (; i < mem_size; i += 4) {
        REG32(dst_base_offset + i) = src_mem_ptr[i >> 2];

        // debug_convert_to_hex(src_mem_ptr[i >> 2], hex_str);

        // uart_print_str(hex_str);
    }
}

/***
    --------------------------- Core Code ---------------------------------
***/
INT8U tile_map[GPU_TILE_MAP_SIZE];

// [16:31] are ammos. One actor can only emit one ammo at most.
struct SpiritStruct spirit_array[SPIRIT_COUNT * 2];

INT32U rand_seed = 2143;
INT32U rand(void) {
   INT32U value;

    //Use a linear congruential generator (LCG) to update the state of the PRNG
    rand_seed *= 1103515245;
    rand_seed += 12345;
    value = (rand_seed >> 16) & 0x07FF;

    rand_seed *= 1103515245;
    rand_seed += 12345;
    value <<= 10;
    value |= (rand_seed >> 16) & 0x03FF;

    rand_seed *= 1103515245;
    rand_seed += 12345;
    value <<= 10;
    value |= (rand_seed >> 16) & 0x03FF;

    //Return the random value
    rand_seed = value;
    return value;
}
// INT32U rand(void) {
//     return 2143;
// }


void init_gpu_memory(void) {
    // copy texture to vram
    INT32U i = 0;
    INT32U temp;
    for (; i < GPU_TEXTURE_SIZE; i += 4) {
        temp = REG32(FLASH_BASE + FLASH_TEXTURE_OFFSET + i);

        REG32(GPU_BASE + GPU_TEXTURE_MEM + i) = temp;
    }
    
    // uart_print_str("VRAM Initialed\n");
    // load tilemap to mem
    INT32U* tile_map_temp_ptr = tile_map;
    i = 0;
    for (; i < GPU_TILE_MAP_SIZE; i += 4) {
        tile_map_temp_ptr[i >> 2] = REG32(FLASH_BASE + FLASH_TILEMAP_OFFSET + i);
    }

    // copy tile map to vram
    gpu_copy_to_vram(GPU_BASE + GPU_TILE_MAP_ARRAY, tile_map_temp_ptr, GPU_TILE_MAP_SIZE);
    
    uart_print_str("VRAM Initialed\n");
}


void get_random_actor(int idx) {
    while (1) {
        spirit_array[idx].position_x = rand() % (GPU_RENDER_WIDTH / 16);
        spirit_array[idx].position_y = rand() % (GPU_RENDER_HEIGHT / 16);
        if (tile_map[spirit_array[idx].position_y * 40 + spirit_array[idx].position_x] >= 10) {
            // unplaceble
            continue;
        }

        spirit_array[idx].position_x = spirit_array[idx].position_x << 4;
        spirit_array[idx].position_y = spirit_array[idx].position_y << 4;

        spirit_array[idx].texture_idx = rand() % 4 + 56; // tank random direction

        break;
    }
}

void generate_me_and_enemy() {
    INT8U i = 0;
    for (; i < SPIRIT_COUNT; ++i) {
        get_random_actor(i);

        spirit_array[i].position_z = i + 1;
    }
    	
    spirit_array[0].texture_idx += 4;
    i = SPIRIT_COUNT;
    for (; i < SPIRIT_COUNT * 2; ++i) {
        spirit_array[i].texture_idx = 36;
        spirit_array[i].position_z = 0;
        spirit_array[i].placeholder = TANK_DIR_INVALID;
    }

    // copy to gpu memory
    gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY, spirit_array, 2 * SPIRIT_COUNT * sizeof(struct SpiritStruct));
    
    // uart_print_str("Gen Spirit\n");
}

INT8U check_close(INT32U pos_1, INT32U pos_2) {
    if (pos_1 > pos_2) {
        if (pos_1 - pos_2 < 4) {
            return 1;
        }
        else {
            return 0;
        }
    }
    else {
        if (pos_2 - pos_1 < 4) {
            return 1;
        }
        else {
            return 0;
        }
    }
}

// return 1 means hit something
INT8U ammo_collision_check(INT8U idx) {
    INT8U tile_x = (spirit_array[idx].position_x + 8) >> 4;
    INT8U tile_y = (spirit_array[idx].position_y + 8) >> 4;
    INT32U map_tile_idx = tile_y * (GPU_RENDER_WIDTH / 16) + tile_x;

    // check hit
    INT32U i = 0;
    for (; i < SPIRIT_COUNT; ++i) {
        if (idx != i + SPIRIT_COUNT && spirit_array[i].position_z != 0) {
            if (check_close(spirit_array[i].position_x, spirit_array[idx].position_x) && check_close(spirit_array[i].position_y, spirit_array[idx].position_y)) {
                spirit_array[i].position_z = 0;

                gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + i * sizeof(struct SpiritStruct), &spirit_array[i], sizeof(struct SpiritStruct));

                return 1;
            }
        }
    }

    INT8U current_texture_idx = tile_map[map_tile_idx];
    if (
        (current_texture_idx >= 16 && current_texture_idx <= 19) || 
        (current_texture_idx == 23) ||
        (current_texture_idx == 30 || current_texture_idx == 31) ||
        (current_texture_idx == 38 || current_texture_idx == 39) ||
        (current_texture_idx == 46 || current_texture_idx == 47)
        ) {
        // hit something cannot be damaged
        return 1;
    }
    if (current_texture_idx >= 48 && current_texture_idx < 52) {
        // can be damaged, replace by grass
        tile_map[map_tile_idx] = 6;
        REG8(GPU_BASE + GPU_TILE_MAP_ARRAY + map_tile_idx) = 6;
        return 1;
    }
    else if (current_texture_idx >= 52 && current_texture_idx < 56) {
        // can be damaged, replace by desert
        tile_map[map_tile_idx] = 4;
        REG8(GPU_BASE + GPU_TILE_MAP_ARRAY + map_tile_idx) = 4;
        return 1;
    }

    return 0;
}

void update_ammo_position() {
    INT8U i = SPIRIT_COUNT;
    for (; i < SPIRIT_COUNT * 2; ++i) {
        if (0 != spirit_array[i].position_z) {
            switch (spirit_array[i].placeholder) {
                case TANK_DIR_RIGHT:
                {
                    spirit_array[i].position_x += AMMO_MOV_SPEED;
                }
                break;

                case TANK_DIR_LEFT:
                {
                    spirit_array[i].position_x -= AMMO_MOV_SPEED;
                }
                break;

                case TANK_DIR_TOP:
                {
                    spirit_array[i].position_y -= AMMO_MOV_SPEED;
                }
                break;

                case TANK_DIR_DOWN:
                {
                    spirit_array[i].position_y += AMMO_MOV_SPEED;
                }
                break;
            }

            if ((spirit_array[i].position_x >> 4) >= 40 || (spirit_array[i].position_y >> 4) >= 30 || ammo_collision_check(i) == 1) {
                spirit_array[i].position_z = 0;
                // uart_print_str("Remove Ammo\n");
                // REG8(GPU_BASE + GPU_SPIRIT_POS_ARRAY + i * sizeof(struct SpiritStruct) + 40) = 0;
            }

            gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + i * sizeof(struct SpiritStruct), &spirit_array[i], sizeof(struct SpiritStruct));
        }
    }
}

// return 1 means collipse something, can't move
INT8U tank_move(INT8U tank_idx, INT8U direction) {
    INT16U new_pos_x = spirit_array[tank_idx].position_x;
    INT16U new_pos_y = spirit_array[tank_idx].position_y;

    switch (direction) {
        case TANK_DIR_RIGHT:
        {
            new_pos_x += TANK_MOV_SPEED;
        }
        break;

        case TANK_DIR_LEFT:
        {
            new_pos_x -= TANK_MOV_SPEED;
        }
        break;

        case TANK_DIR_TOP:
        {
            new_pos_y -= TANK_MOV_SPEED;
        }
        break;

        case TANK_DIR_DOWN:
        {
            new_pos_y += TANK_MOV_SPEED;
        }
        break;
    }

    INT8U new_tile_x = new_pos_x >> 4;
    INT8U new_tile_y = new_pos_y >> 4;
    if (new_tile_x >= 40) {
        return 1;
    }
    if (new_tile_y >= 30) {
        return 1;
    }

    INT16U left_up_idx = 40 * new_tile_y + new_tile_x;
    if (tile_map[left_up_idx] >= 10) {
        return 1;
    }

    if (tile_map[left_up_idx + 1] >= 10) {
        return 1;
    }

    if (tile_map[left_up_idx + 40] >= 10) {
        return 1;
    }

    if (tile_map[left_up_idx + 41] >= 10) {
        return 1;
    }


    spirit_array[tank_idx].position_x = new_pos_x;

    spirit_array[tank_idx].position_y = new_pos_y;

    INT8U new_direction = (spirit_array[tank_idx].texture_idx & 0xfc) | direction;
    spirit_array[tank_idx].texture_idx = new_direction;

    gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + tank_idx * sizeof(struct SpiritStruct), &spirit_array[tank_idx], sizeof(struct SpiritStruct));

    return 0;
}

void fire(INT8U idx) {
    INT8U ammo_idx = idx + SPIRIT_COUNT;
    if (spirit_array[ammo_idx].position_z == 0) {
        spirit_array[ammo_idx].position_z = 250;
        spirit_array[ammo_idx].texture_idx = 36;
        spirit_array[ammo_idx].position_x = spirit_array[idx].position_x;
        spirit_array[ammo_idx].position_y = spirit_array[idx].position_y;
        spirit_array[ammo_idx].placeholder = spirit_array[idx].texture_idx & 3;

        gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + ammo_idx * sizeof(struct SpiritStruct), &spirit_array[ammo_idx], sizeof(struct SpiritStruct));
    }
}

void AI_move(INT8U enemy_idx) {
    if (rand() % 64 > 48) {
        // 75%概率不移动
        return;
    }

    INT8U enemy_idx_tile_x = spirit_array[enemy_idx].position_x >> 4;
    INT8U enemy_idx_tile_y = spirit_array[enemy_idx].position_y >> 4;

    INT8U player_idx_tile_x = spirit_array[0].position_x >> 4;
    INT8U player_idx_tile_y = spirit_array[0].position_y >> 4;

    if (enemy_idx_tile_x == player_idx_tile_x) {
        if (enemy_idx_tile_y < player_idx_tile_y) {
            INT8U can_fire = 1;
            INT8U tile_y = enemy_idx_tile_y;
            for (; tile_y < player_idx_tile_y; ++tile_y) {
                if (tile_map[tile_y + enemy_idx_tile_x * 40] >= 10 && tile_map[tile_y + enemy_idx_tile_x * 40] < 48) {
                    can_fire = 0;
                    break;
                }
            }

            if (can_fire == 1) {
                // rotate to player
                INT8U new_direction = (spirit_array[enemy_idx].texture_idx & 0xfc) | TANK_DIR_DOWN;
                spirit_array[enemy_idx].texture_idx = new_direction;
                // REG16(GPU_BASE + GPU_SPIRIT_POS_ARRAY + enemy_idx * sizeof(struct SpiritStruct) + 32) = new_direction;
                gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + enemy_idx * sizeof(struct SpiritStruct), &spirit_array[enemy_idx], sizeof(struct SpiritStruct));

                // fire to player
                fire(enemy_idx);

                return;
            }
        }
        else {
            INT8U can_fire = 1;
            INT8U tile_y = player_idx_tile_y;
            for (; tile_y < enemy_idx_tile_y; ++tile_y) {
                if (tile_map[tile_y + enemy_idx_tile_x * 40] >= 10 && tile_map[tile_y + enemy_idx_tile_x * 40] < 48) {
                    can_fire = 0;
                    break;
                }
            }

            if (can_fire == 1) {
                // rotate to player
                INT8U new_direction = (spirit_array[enemy_idx].texture_idx & 0xfc) | TANK_DIR_TOP;
                spirit_array[enemy_idx].texture_idx = new_direction;
                gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + enemy_idx * sizeof(struct SpiritStruct), &spirit_array[enemy_idx], sizeof(struct SpiritStruct));

                // fire to player
                fire(enemy_idx);

                return;
            }
        }
    }

    if (enemy_idx_tile_y == player_idx_tile_y) {
        if (enemy_idx_tile_x < player_idx_tile_x) {
            INT8U can_fire = 1;
            INT8U tile_x = enemy_idx_tile_x;
            for (; tile_x < player_idx_tile_x; ++tile_x) {
                if (tile_map[tile_x * 40 + enemy_idx_tile_y] >= 10 && tile_map[tile_x * 40 + enemy_idx_tile_y] < 48) {
                    can_fire = 0;
                    break;
                }
            }

            if (can_fire == 1) {
                // rotate to player
                INT8U new_direction = (spirit_array[enemy_idx].texture_idx & 0xfc) | TANK_DIR_RIGHT;
                spirit_array[enemy_idx].texture_idx = new_direction;
                gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + enemy_idx * sizeof(struct SpiritStruct), &spirit_array[enemy_idx], sizeof(struct SpiritStruct));

                // fire to player
                fire(enemy_idx);

                return;
            }
        }
        else {
            INT8U can_fire = 1;
            INT8U tile_x = player_idx_tile_x;
            for (; tile_x < enemy_idx_tile_x; ++tile_x) {
                if (tile_map[tile_x * 40 + enemy_idx_tile_y] >= 10 && tile_map[tile_x * 40 + enemy_idx_tile_y] < 48) {
                    can_fire = 0;
                    break;
                }
            }

            if (can_fire == 1) {
                // rotate to player
                INT8U new_direction = (spirit_array[enemy_idx].texture_idx & 0xfc) | TANK_DIR_LEFT;
                spirit_array[enemy_idx].texture_idx = new_direction;
                gpu_copy_to_vram(GPU_BASE + GPU_SPIRIT_POS_ARRAY + enemy_idx * sizeof(struct SpiritStruct), &spirit_array[enemy_idx], sizeof(struct SpiritStruct));

                // fire to player
                fire(enemy_idx);

                return;
            }
        }
    }


    // random walk
    INT8U walk_random = rand() % 64;
    if (walk_random > 16) {
        // 48 / 64的概率向前走
        tank_move(enemy_idx, spirit_array[enemy_idx].texture_idx & 0x3);
    }
    else {
        tank_move(enemy_idx, rand() % 4);
    }
}

void game_loop() {
    INT8U command = 'W';
    INT8U prev_command = 'W';
    INT32U same_command_cnt = 0;
    INT16U update_cnt = 0;
    INT32U ai_tick_cnt = 0;

    while (spirit_array[0].position_z > 0) {
        command = gpio_in() & 0x1f;

        if (prev_command != command || (prev_command != 0 && same_command_cnt > 300)) {
            prev_command = command;
            same_command_cnt = 0;

            switch (command) {
                case BUTTON_UP:
                {
                    tank_move(0, TANK_DIR_TOP);
                }
                break;

                case BUTTON_LEFT:
                {
                    tank_move(0, TANK_DIR_LEFT);
                }
                break;

                case BUTTON_DOWN:
                {
                    tank_move(0, TANK_DIR_DOWN);
                }
                break;

                case BUTTON_RIGHT:
                {
                    tank_move(0, TANK_DIR_RIGHT);
                }
                break;

                case BUTTON_FIRE:
                {
                    fire(0);
                }
                break;
            }
        }
        else {
            ++same_command_cnt;
        }


        if (update_cnt > 20) {
            update_ammo_position();
            update_cnt = 0;
        }
        else {
            ++update_cnt;
        }

        if (ai_tick_cnt > 4000) {
            int i = 1;
            for (; i < SPIRIT_COUNT; ++i) {
                if (spirit_array[i].position_z > 0) {
                    AI_move(i);
                }
            }

            ai_tick_cnt = 0;
        }
        else {
            ++ai_tick_cnt;
        }
    }
}

void  TaskStart (void *pdata)
{
    pdata = pdata;            /* Prevent compiler warning                 */
    OSInitTick();	      /* don't put this function in main()        */       
    

    rand_seed = gpio_in();

    init_gpu_memory();

    generate_me_and_enemy();

    uart_print_str("Render Prepared\n");
    REG8(GPU_BASE + GPU_SPIRIT_CNT_REG) = SPIRIT_COUNT * 2;
    gpu_start_render();

    uart_print_str("Enter Loop\n");
    game_loop();

    gpu_stop_render();

    uart_print_str("Render End\n");
}

void main()
{
  OSInit();
  
  uart_init();
  
  gpio_init();	
  
  OSTaskCreate(TaskStart, 
	       (void *)0, 
	       &TaskStartStk[TASK_STK_SIZE - 1], 
	       0);
  
  OSStart();  
  
}

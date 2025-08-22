#include "led.h"
#include <vector>
#include <map>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstdio>

void update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16)
{
    uint8_t display_buffer[16] = {0};
    for (int digit_index = 0; digit_index < (int)grid16.size(); ++digit_index) {
        uint16_t bitmask = grid16[digit_index];
        for (int seg=0; seg<8; ++seg) {
            if ((bitmask >> seg) & 1) {
                int base = seg*2;
                int addr_to_write, bit_pos;
                if (digit_index < 8) {
                    addr_to_write = base;
                    bit_pos = digit_index;
                } else {
                    addr_to_write = base+1;
                    bit_pos = digit_index-8;
                }
                display_buffer[addr_to_write] |= (1 << bit_pos);
            }
        }
    }
    if (ioctl(i2c_bus_fd, I2C_SLAVE, addr) < 0) {
        perror("ioctl I2C_SLAVE");
        return;
    }
    uint8_t buf[17];
    buf[0] = 0x00;
    memcpy(buf+1, display_buffer, 16);
    if (write(i2c_bus_fd, buf, 17) != 17) {
        perror("i2c write");
    }
}

void update_display(int i2c_fd,
                    const std::vector<uint8_t>& grid,
                    const std::vector<int>& module_addrs)
{
    constexpr int ACROSS = 24;
    std::map<int,std::vector<uint8_t>> modules;
    for(int addr: module_addrs) modules[addr]=std::vector<uint8_t>(16,0);

    for(int i=0;i<(int)grid.size();i++){
        int row=i/ACROSS, col=i%ACROSS;
        int module_index=col/4;
        if(module_index < (int)module_addrs.size()){
            int target_addr=module_addrs[module_index];
            int col_in_module=col%4;
            int digit_index_on_module=row*4+col_in_module;
            modules[target_addr][digit_index_on_module]=grid[i];
        }
    }
    for(auto &kv: modules){
        update_module_from_grid(i2c_fd, kv.first, kv.second);
    }
}

from pathlib import Path
import subprocess
import argparse,tempfile
ap=argparse.ArgumentParser();ap.add_argument('--cc',required=True);args=ap.parse_args()
root=Path(__file__).resolve().parents[1]
temp=tempfile.TemporaryDirectory(prefix='offline-clock-');out=Path(temp.name)
source=(root/'src/storage/offline_clock.c').read_text(encoding='utf-8')
source='\n'.join(l for l in source.splitlines() if not l.startswith('#include'))
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#define OFFLINE_TIME_LIMIT 946684800U
#define OFFLINE_CLOCK_WIRE_SIZE 24U
#define K_FOREVER 0
static unsigned char saved[24];
static int saved_len=-ENOENT, fail_write, ota_busy;
static int ble_ota_in_progress(void){return ota_busy;}
static int nvs_config_is_ready(void){return 1;}
static void *w25q64_get_mutex(void){return 0;}
static void k_mutex_lock(void *p,int x){}
static void k_mutex_unlock(void *p){}
static void *nvs_get_fs(void){return 0;}
static int nvs_read(void *fs,unsigned key,void *p,unsigned n){if(saved_len>=0)memcpy(p,saved,n);return saved_len;}
static int nvs_write(void *fs,unsigned key,const void *p,unsigned n){if(fail_write)return -EIO;memcpy(saved,p,n);saved_len=n;return n;}
static void sys_put_le32(uint32_t v,uint8_t *p){for(int i=0;i<4;i++)p[i]=(v>>(i*8))&255;}
'''
tests=r'''
int main(void){
 assert(offline_clock_boot()==0 && boot_base==1);
 assert(offline_clock_now(600)==601);
 assert(offline_clock_anchor(1792100000U,1800)==0);
 assert(clock_state.anchor_base==1 && clock_state.anchor_uptime==1800);
 assert(offline_clock_anchor(1792101000U,2800)==0);
 assert(clock_state.anchor_unix==1792100000U);
 boot_base=0; assert(offline_clock_boot()==0 && boot_base==3601);
 assert(clock_state.anchor_base==1); /* Retain prior anchor across power loss before App reads it. */
 assert(offline_clock_now(600)==4201);
 ota_busy=1; assert(offline_clock_now(10000)==0);ota_busy=0;
 fail_write=1; assert(offline_clock_now(10000)==0);
 fail_write=0; assert(offline_clock_now(10000)==13601);
 uint32_t end=clock_state.high_water;
 boot_base=0; assert(offline_clock_boot()==0 && boot_base>end);
 fail_write=1; assert(offline_clock_anchor(1792200000U,1000)<0 && clock_state.anchor_base==1);
 fail_write=0; assert(offline_clock_anchor(1792200000U,1000)==0);
 uint32_t mapped=clock_state.anchor_base;
 boot_base=0; assert(offline_clock_boot()==0 && clock_state.anchor_base==mapped);
 assert(offline_clock_anchor(100,1)<0);
 fail_write=1; boot_base=0; assert(offline_clock_boot()<0 && boot_base==0 && offline_clock_now(1)==0);
 fail_write=0; saved[4]^=1;assert(offline_clock_boot()<0 && boot_base==0);saved[4]^=1; saved[0]=0; assert(offline_clock_boot()<0 && boot_base==0);
 clock_state.magic=CLOCK_MAGIC;clock_state.high_water=OFFLINE_TIME_LIMIT-1;
 clock_state.checksum=clock_checksum();memcpy(saved,&clock_state,sizeof clock_state);assert(offline_clock_boot()==-ENOSPC);
 puts("PASS: unique boot ranges, long offline extension, first anchor immutability, prior anchor recovery, atomic failure, corrupt/exhausted allocator");
 return 0;
}
'''
c=out/'offline-clock-test.c';c.write_text(prefix+source+tests,encoding='utf-8')
tcc=Path(args.cc).resolve()
exe=out/'offline-clock-test.exe'
subprocess.run([str(tcc),str(c.resolve()),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
temp.cleanup()

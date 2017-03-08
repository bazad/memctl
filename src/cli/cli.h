#ifndef MEMCTL_CLI__CLI_H_
#define MEMCTL_CLI__CLI_H_

#include "cli/command.h"
#include "cli/disassemble.h"

bool default_action(void);

bool r_command(kaddr_t address, size_t length, bool physical, size_t width, size_t access,
		bool dump);
bool rb_command(kaddr_t address, size_t length, bool physical, size_t access);
#if MEMCTL_DISASSEMBLY
bool ri_command(kaddr_t address, size_t length, bool physical, size_t access);
bool rif_command(const char *function, const char *kext, size_t access);
#endif
bool rs_command(kaddr_t address, size_t length, bool physical, size_t access);
bool w_command(kaddr_t address, kword_t value, bool physical, size_t width, size_t access);
bool wd_command(kaddr_t address, const void *data, size_t length, bool physical, size_t access);
bool ws_command(kaddr_t address, const char *string, bool physical, size_t access);
bool f_command(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical,
		size_t access, size_t alignment);
bool fpr_command(pid_t pid);
bool fi_command(kaddr_t start, kaddr_t end, const char *classname, const char *bundle_id,
		size_t access);
bool kp_command(kaddr_t address);
bool kpm_command(kaddr_t start, kaddr_t end);
bool vt_command(const char *classname, const char *bundle_id);
bool vm_command(kaddr_t address, unsigned depth);
bool vmm_command(kaddr_t start, kaddr_t end, unsigned depth);
bool ks_command(kaddr_t address, bool unslide);
bool a_command(const char *symbol, const char *kext);
bool ap_command(kaddr_t address, bool unpermute);
bool s_command(kaddr_t address);
bool kcd_command(const char *kernelcache, const char *output);

#endif

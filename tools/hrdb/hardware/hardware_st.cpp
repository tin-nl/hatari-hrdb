#include "hardware_st.h"
#include "regs_st.h"

#include "../models/memory.h"
#include "../models/targetmodel.h"

namespace HardwareST
{
    bool GetVideoBase(const Memory& mem, MACHINETYPE machineType, uint32_t& address)
    {
        if (!mem.HasAddress(Regs::VID_SCREEN_HIGH))
            return false;
        if (!mem.HasAddress(Regs::VID_SCREEN_MID))
            return false;
        uint32_t hi = mem.ReadAddressByte(Regs::VID_SCREEN_HIGH);
        uint32_t mi = mem.ReadAddressByte(Regs::VID_SCREEN_MID);
        uint32_t lo = 0;
        if (!IsMachineST(machineType))
        {
            if (!mem.HasAddress(Regs::VID_SCREEN_LOW_STE))
                return false;
            lo = mem.ReadAddressByte(Regs::VID_SCREEN_LOW_STE);
        }
        address = (hi << 16) | (mi << 8) | lo;
        return true;
    }
}

#include "hardware_st.h"
#include "regs_st.h"

#include "../models/memory.h"
#include "../models/targetmodel.h"

namespace HardwareST
{
    bool GetVideoBase(const Memory& mem, MACHINETYPE machineType, uint32_t& address)
    {
        if (mem.GetSize() > 0)
        {
            uint32_t hi = mem.ReadAddressByte(Regs::VID_SCREEN_HIGH);
            uint32_t mi = mem.ReadAddressByte(Regs::VID_SCREEN_MID);
            uint32_t lo = mem.ReadAddressByte(Regs::VID_SCREEN_LOW_STE);
            if (IsMachineST(machineType))
                lo = 0;
            address = (hi << 16) | (mi << 8) | lo;
            return true;
        }
        return false;
    }
}

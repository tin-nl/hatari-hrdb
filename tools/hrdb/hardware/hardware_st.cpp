#include "hardware_st.h"
#include "regs_st.h"

#include "../models/memory.h"
#include "../models/targetmodel.h"

namespace HardwareST
{
    bool GetVideoBase(const Memory& mem, MACHINETYPE machineType, uint32_t& address)
    {
        if (!mem.HasAddress(Regs::VID_BASE_HIGH))
            return false;
        if (!mem.HasAddress(Regs::VID_BASE_MID))
            return false;
        uint32_t hi = mem.ReadAddressByte(Regs::VID_BASE_HIGH);
        uint32_t mi = mem.ReadAddressByte(Regs::VID_BASE_MID);
        uint32_t lo = 0;
        if (!IsMachineST(machineType))
        {
            if (!mem.HasAddress(Regs::VID_BASE_LOW_STE))
                return false;
            lo = mem.ReadAddressByte(Regs::VID_BASE_LOW_STE);
        }
        address = (hi << 16) | (mi << 8) | lo;
        return true;
    }

    bool GetVideoCurrent(const Memory& mem, uint32_t& address)
    {
        if (!mem.HasAddress(Regs::VID_CURR_HIGH))
            return false;
        if (!mem.HasAddress(Regs::VID_CURR_MID))
            return false;
        if (!mem.HasAddress(Regs::VID_CURR_LOW))
            return false;
        uint32_t hi = mem.ReadAddressByte(Regs::VID_CURR_HIGH);
        uint32_t mi = mem.ReadAddressByte(Regs::VID_CURR_MID);
        uint32_t lo = mem.ReadAddressByte(Regs::VID_CURR_LOW);
        address = (hi << 16) | (mi << 8) | lo;
        return true;
    }

    bool GetBlitterSrc(const Memory &mem, MACHINETYPE machineType, uint32_t &address)
    {
        if (IsMachineST(machineType))
            return false;

        if (!mem.HasAddressMulti(Regs::BLT_SRC_ADDR, 4))
            return false;
        uint32_t val = mem.ReadAddressMulti(Regs::BLT_SRC_ADDR, 4);
        address = val & 0xffffff;
        return true;
    }

    bool GetBlitterDst(const Memory &mem, MACHINETYPE machineType, uint32_t &address)
    {
        if (IsMachineST(machineType))
            return false;

        if (!mem.HasAddressMulti(Regs::BLT_DST_ADDR, 4))
            return false;
        uint32_t val = mem.ReadAddressMulti(Regs::BLT_DST_ADDR, 4);
        address = val & 0xffffff;
        return true;
    }
}

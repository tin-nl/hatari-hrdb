#include "symboltable.h"
#include <assert.h>
#include <algorithm>

#define ADD_SYM(symname, addr, size)  AddInternal(#symname, addr, size);

class SymbolNameCompare
{
public:
    // This is the "less" comparison
    bool operator()(const Symbol &lhs, const Symbol &rhs) const
    {
        return lhs.name < rhs.name;
    }
};

void SymbolTable::AddHardware()
{
    ADD_SYM(vid_memconf		, 0xff8001, 1)	// memory controller
    ADD_SYM(vid_dbaseh		, 0xff8201, 1)
    ADD_SYM(vid_dbasel		, 0xff8203, 1)	// display base low, high
    ADD_SYM(vid_vcounthi	, 0xff8205, 1)	// display counter low, medium, high
    ADD_SYM(vid_vcountmid	, 0xff8207, 1)
    ADD_SYM(vid_vcountlow	, 0xff8209, 1)
    ADD_SYM(vid_syncmode	, 0xff820a, 1)	// video	sync mode
    ADD_SYM(vid_color0		, 0xff8240, 2)	// color registers 0..15
    ADD_SYM(vid_color1		, 0xff8242, 2)
    ADD_SYM(vid_color2		, 0xff8244, 2)
    ADD_SYM(vid_color3		, 0xff8246, 2)
    ADD_SYM(vid_color4		, 0xff8248, 2)
    ADD_SYM(vid_color5		, 0xff824a, 2)
    ADD_SYM(vid_color6		, 0xff824c, 2)
    ADD_SYM(vid_color7		, 0xff824e, 2)
    ADD_SYM(vid_color8		, 0xff8250, 2)
    ADD_SYM(vid_color9		, 0xff8252, 2)
    ADD_SYM(vid_color10		, 0xff8254, 2)
    ADD_SYM(vid_color11		, 0xff8256, 2)
    ADD_SYM(vid_color12		, 0xff8258, 2)
    ADD_SYM(vid_color13		, 0xff825a, 2)
    ADD_SYM(vid_color14		, 0xff825c, 2)
    ADD_SYM(vid_color15		, 0xff825e, 2)
    ADD_SYM(vid_shiftmd		, 0xff8260, 1)	// shifter mode (resolution)
    ADD_SYM(dma_diskctl		, 0xff8604, 1)	// disk controller data access
    ADD_SYM(dma_fifo		, 0xff8606, 1)	// DMA mode control
    ADD_SYM(dma_dmahigh		, 0xff8609, 1)	// DMA base high, medium, low
    ADD_SYM(dma_dmamid		, 0xff860b, 1)
    ADD_SYM(dma_dmalow		, 0xff860d, 1)
    ADD_SYM(ym_giselect     , 0xff8800, 1)	// (W) sound chip register select
    ADD_SYM(ym_giread		, 0xff8800, 1)	// (R) sound chip read-data
    ADD_SYM(ym_giwrite		, 0xff8802, 1)	// (W) sound chip write-data
    ADD_SYM(mfp_gpip        , 0xfffa00+1	, 1)		// general purpose I/O
    ADD_SYM(mfp_aer         , 0xfffa00+3	, 1)		// active edge reg
    ADD_SYM(mfp_ddr         , 0xfffa00+5	, 1)		// data direction reg
    ADD_SYM(mfp_iera        , 0xfffa00+7	, 1)		// interrupt enable A & B
    ADD_SYM(mfp_ierb        , 0xfffa00+9    , 1)
    ADD_SYM(mfp_ipra        , 0xfffa00+0xb	, 1)	// interrupt pending A & B
    ADD_SYM(mfp_iprb        , 0xfffa00+0xd  , 1)
    ADD_SYM(mfp_isra        , 0xfffa00+0xf	, 1)	// interrupt inService A & B
    ADD_SYM(mfp_isrb        , 0xfffa00+0x11 , 1)
    ADD_SYM(mfp_imra        , 0xfffa00+0x13	, 1)	// interrupt mask A & B
    ADD_SYM(mfp_imrb        , 0xfffa00+0x15 , 1)
    ADD_SYM(mfp_vr          , 0xfffa00+0x17	, 1)	// interrupt vector base
    ADD_SYM(mfp_tacr        , 0xfffa00+0x19	, 1)	// timer A control
    ADD_SYM(mfp_tbcr        , 0xfffa00+0x1b	, 1)	// timer B control
    ADD_SYM(mfp_tcdcr       , 0xfffa00+0x1d	, 1)	// timer C & D control
    ADD_SYM(mfp_tadr        , 0xfffa00+0x1f	, 1)	// timer A data
    ADD_SYM(mfp_tbdr        , 0xfffa00+0x21	, 1)	// timer B data
    ADD_SYM(mfp_tcdr        , 0xfffa00+0x23	, 1)	// timer C data
    ADD_SYM(mfp_tddr        , 0xfffa00+0x25	, 1)	// timer D data
    ADD_SYM(mfp_scr         , 0xfffa00+0x27	, 1)	// sync char
    ADD_SYM(mfp_ucr         , 0xfffa00+0x29	, 1)	// USART control reg
    ADD_SYM(mfp_rsr         , 0xfffa00+0x2b	, 1)	// receiver status
    ADD_SYM(mfp_tsr         , 0xfffa00+0x2d	, 1)	// transmit status
    ADD_SYM(mfp_udr         , 0xfffa00+0x2f	, 1)	// USART data
    ADD_SYM(acia_keyctl     , 0xfffc00, 1)		// keyboard ACIA control
    ADD_SYM(acia_keybd      , 0xfffc02, 1)		// keyboard data
    ADD_SYM(acia_midictl	, 0xfffc04, 1)		// MIDI ACIA control
    ADD_SYM(acia_midi       , 0xfffc06, 1)		// MIDI data

    ADD_SYM(etv_timer       , 0x400, 4)	// vector for timer interrupt chain
    ADD_SYM(etv_critic      , 0x404, 4)	// vector for critical error chain
    ADD_SYM(etv_term        , 0x408, 4)	// vector for process terminate
    ADD_SYM(etv_xtra        , 0x40c, 20)	// 5 reserved vectors
    ADD_SYM(memvalid        , 0x420, 4)	// indicates system state on RESET
    ADD_SYM(memcntlr        , 0x424, 2)	// mem controller config nibble
    ADD_SYM(resvalid        , 0x426, 4)	// validates 'resvector'
    ADD_SYM(resvector       , 0x42a, 4)	// [RESET] bailout vector
    ADD_SYM(phystop         , 0x42e, 4)	// physical top of RAM
    ADD_SYM(_membot         , 0x432, 4)	// bottom of available memory//
    ADD_SYM(_memtop         , 0x436, 4)	// top of available memory//
    ADD_SYM(memval2         , 0x43a, 4)	// validates 'memcntlr' and 'memconf'
    ADD_SYM(flock           , 0x43e, 2)	// floppy disk/FIFO lock variable
    ADD_SYM(seekrate        , 0x440, 2)	// default floppy seek rate
    ADD_SYM(_timr_ms        , 0x442, 2)	// system timer calibration (in ms)
    ADD_SYM(_fverify        , 0x444, 2)	// nonzero: verify on floppy write
    ADD_SYM(_bootdev        , 0x446, 2)	// default boot device
    ADD_SYM(palmode         , 0x448, 2)	// nonzero ==> PAL mode
    ADD_SYM(defshiftmd      , 0x44a, 2)	// default video rez (first byte)
    ADD_SYM(sshiftmd        , 0x44c, 2)	// shadow for 'shiftmd' register
    ADD_SYM(_v_bas_ad       , 0x44e, 4)	// pointer to base of screen memory
    ADD_SYM(vblsem          , 0x452, 2)	// semaphore to enforce mutex in	vbl
    ADD_SYM(nvbls           , 0x454, 4)	// number of deferred vectors
    ADD_SYM(_vblqueue       , 0x456, 4)	// pointer to vector of deferred	vfuncs
    ADD_SYM(colorptr        , 0x45a, 4)	// pointer to palette setup (or NULL)
    ADD_SYM(screenpt        , 0x45e, 4)	// pointer to screen base setup (|NULL)
    ADD_SYM(_vbclock        , 0x462, 4)	// count	of unblocked vblanks
    ADD_SYM(_frclock        , 0x466, 4)	// count	of every vblank
    ADD_SYM(hdv_init        , 0x46a, 4)	// hard disk initialization
    ADD_SYM(swv_vec         , 0x46e, 4)	// video change-resolution bailout
    ADD_SYM(hdv_bpb         , 0x472, 4)	// disk "get BPB"
    ADD_SYM(hdv_rw          , 0x476, 4)	// disk read/write
    ADD_SYM(hdv_boot        , 0x47a, 4)	// disk "get boot sector"
    ADD_SYM(hdv_mediach     , 0x47e, 4)	// disk media change detect
    ADD_SYM(_cmdload        , 0x482, 2)	// nonzero: load COMMAND.COM from boot
    ADD_SYM(conterm         , 0x484, 2)	// console/vt52 bitSwitches (%%0..%%2)
    ADD_SYM(trp14ret        , 0x486, 4)	// saved return addr for _trap14
    ADD_SYM(criticret       , 0x48a, 4)	// saved return addr for _critic
    ADD_SYM(themd           , 0x48e, 4)	// memory descriptor (MD)
    ADD_SYM(_____md         , 0x49e, 4)	// (more MD)
    ADD_SYM(savptr          , 0x4a2, 4)	// pointer to register save area
    ADD_SYM(_nflops         , 0x4a6, 2)	// number of disks attached (0, 1+)
    ADD_SYM(con_state       , 0x4a8, 4)	// state of conout() parser
    ADD_SYM(save_row        , 0x4ac, 2)	// saved row# for cursor X-Y addressing
    ADD_SYM(sav_context     , 0x4ae, 4)	// pointer to saved processor context
    ADD_SYM(_bufl           , 0x4b2, 8)	// two buffer-list headers
    ADD_SYM(_hz_200         , 0x4ba, 4)	// 200hz raw system timer tick
    ADD_SYM(_drvbits        , 0x4c2, 4)	// bit vector of "live" block devices
    ADD_SYM(_dskbufp        , 0x4c6, 4)	// pointer to common disk buffer
    ADD_SYM(_autopath       , 0x4ca, 4)	// pointer to autoexec path (or NULL)
    ADD_SYM(_vbl_list       , 0x4ce, 4)	// initial _vblqueue (to $4ee)
    ADD_SYM(_dumpflg        , 0x4ee, 2)	// screen-dump flag
    ADD_SYM(_prtabt         , 0x4f0, 4)	// printer abort flag
    ADD_SYM(_sysbase        , 0x4f2, 4)	// -> base of OS
    ADD_SYM(_shell_p        , 0x4f6, 4)	// -> global shell info
    ADD_SYM(end_os          , 0x4fa, 4)	// -> end of OS memory usage
    ADD_SYM(exec_os         , 0x4fe, 4)	// -> address of shell to exec on startup
    ADD_SYM(scr_dump        , 0x502, 4)	// -> screen dump code
    ADD_SYM(prv_lsto        , 0x506, 4)	// -> _lstostat()
    ADD_SYM(prv_lst         , 0x50a, 4)	// -> _lstout()
    ADD_SYM(prv_auxo        , 0x50e, 4)	// -> _auxostat()
    ADD_SYM(prv_aux         , 0x512, 4)	// -> _auxout()
    ADD_SYM(user_mem        ,0x1000, 0)

    ADD_SYM(tos        ,0xe00000, 256 * 1024)
}

SymbolTable::SymbolTable() :
    m_userSymbolCount(0)
{

    AddHardware();
    AddComplete();
    m_userSymbolCount = 0; // reset after Add()
}

void SymbolTable::AddInternal(const Symbol &sym)
{
    size_t index = m_symbols.size();
    m_addrLookup.insert(Pair(sym.address, index));
    m_symbols.push_back(sym);
    ++m_userSymbolCount;
}

void SymbolTable::AddComplete()
{
    // Sort the symbols in name order
    std::sort(m_symbols.begin(), m_symbols.end(), SymbolNameCompare());

    // Recalc the keys table
    m_addrKeys.clear();
    Map::iterator it(m_addrLookup.begin());
    while (it != m_addrLookup.end())
    {
        m_addrKeys.push_back(*it);
        ++it;
    }
}

bool SymbolTable::Find(uint32_t address, Symbol &result) const
{
    Map::const_iterator it = m_addrLookup.find(address);
    if (it == m_addrLookup.end())
        return false;

    result = m_symbols[it->second];
    return true;
}

bool SymbolTable::FindLowerOrEqual(uint32_t address, Symbol &result) const
{
    // Degenerate cases
    if (m_addrKeys.size() == 0)
        return false;

    if (address < m_symbols[m_addrKeys[0].second].address)
        return false;

    // Binary chop
    size_t lower = 0;
    size_t upper_plus_one = m_addrKeys.size();

    // We need to find the first item which is *lower or equal* to N,
    while ((lower + 1) < upper_plus_one)
    {
        size_t mid = (lower + upper_plus_one) / 2;

        const Symbol& midSym = m_symbols[m_addrKeys[mid].second];
        if (midSym.address <= address)
        {
            // mid is lower/equal, search
            lower = mid;
        }
        else {
            // mid is
            upper_plus_one = mid;
        }
    }

    result = m_symbols[m_addrKeys[lower].second];
    assert(address >= result.address);
    // Size checks
    if (result.size == 0)
        return true;        // unlimited size
    else if (result.size > (address - result.address))
        return true;
    return false;
}

bool SymbolTable::Find(std::string name, Symbol &result) const
{
    for (size_t i = 0; i < this->m_symbols.size(); ++i)
    {
        if (m_symbols[i].name == name)
        {
            result = m_symbols[i];
            return true;
        }
    }
    return false;
}

const Symbol& SymbolTable::Get(size_t index) const
{
    return m_symbols[index];
}

void SymbolTable::AddInternal(const char* name, uint32_t addr, uint32_t size)
{
    Symbol sym;
    sym.name = name;
    sym.address = addr;
    sym.type = "H";     // hardware
    sym.size = size;
    AddInternal(sym);
}

Symbol::Symbol(std::string nameArg, uint32_t addressArg) :
    name(nameArg), address(addressArg)
{

}

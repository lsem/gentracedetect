#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <cassert>
#include <cstdio>
#include <stdexcept>

using std::string;


//////////////////////////////////////////////////////////////////////////

#define EXECSAMPLECODE_ADDRESS		NULL
#define EXECSAMPLECODE_SIZEBYTES	(4096 * 16)         // Try to fit it to the L1


extern const uint8_t g_sample_code_program_prolog[];
extern const uint8_t g_sample_code_program_epilog[];
extern const uint8_t g_sample_program_code_body[];

extern const size_t g_epilog_size;
extern const size_t g_body_size;
extern const size_t g_prolog_size;


void show_failure_message(const string &user_message);
string format_results_message(int result_value, unsigned timeTaken);


bool create_sample_memory_region(void **p_out_region_address)
{
	bool result = false;

	HANDLE current_process_handle = GetCurrentProcess();
	const auto flags = MEM_COMMIT | MEM_RESERVE;
	const size_t region_size = EXECSAMPLECODE_SIZEBYTES;
	void *p_allocationAddress;
	
	do 
	{
		p_allocationAddress = ::VirtualAllocEx(current_process_handle, EXECSAMPLECODE_ADDRESS, region_size, flags, PAGE_EXECUTE_READWRITE);
		if (p_allocationAddress == NULL)
		{
			show_failure_message("Virtual Alloc Failed");
			break;
		}

		DWORD old_protect_flags;
		if (!::VirtualProtectEx(current_process_handle, p_allocationAddress, region_size, PAGE_EXECUTE_READWRITE, &old_protect_flags))
		{
			show_failure_message("Virtual Protect Failed");
			break;
		}

		if (!::FlushInstructionCache(current_process_handle, p_allocationAddress, region_size))
		{
			show_failure_message("Flushing instructions cache failed");
			break;
		}

		*p_out_region_address = p_allocationAddress;

		result = true;
	} 
	while (false);

	if (!result)
	{
		if (p_allocationAddress != NULL)
		{
			VirtualFreeEx(current_process_handle,  p_allocationAddress, region_size, MEM_RELEASE);
		}
	}

	return result;
}

void generate_sample_code(void *destination, size_t destination_size)
{
	size_t offset = 0;

	::memcpy(destination, &g_sample_code_program_prolog[0], g_prolog_size);
	offset += g_prolog_size;

	const size_t body_chunk_size = g_body_size;
	size_t body_high = destination_size - sizeof(g_prolog_size);

	while ((body_high - offset) > body_chunk_size)
	{
		void *next_block_start = (void *) ((uintptr_t) destination + offset);
		::memcpy(next_block_start, &g_sample_program_code_body[0], body_chunk_size);
		offset += body_chunk_size;		
	}

	void *last_block_start = (void *) ((uintptr_t) destination + offset);
	::memcpy(last_block_start, &g_sample_code_program_epilog[0], g_epilog_size);
	offset += g_epilog_size;

	assert(offset < destination_size);
}

//////////////////////////////////////////////////////////////////////////

void show_failure_message(const string &user_message)
{
	DWORD last_error = GetLastError();
	std::stringstream ss;
	ss << user_message << "\nLast error: " << last_error; 
	MessageBox(NULL, ss.str().c_str(), "Failure", MB_ICONERROR);	
}


string format_results_message(int result_value, unsigned timeTaken)
{		
	std::stringstream ss;
	ss << "Result: " << result_value << std::endl;
	ss << "Time Taken: " << timeTaken;

	return ss.str();
}

typedef int (*sample_code_func_t)(int input);


//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////

void finalize_testing_mode();

class current_process
{
public:
    static bool initialize_testing_mode()
    {
        bool result = false;

        HANDLE process_handle = ::GetCurrentProcess();
        HANDLE thread_handle = ::GetCurrentThread();

        do
        {
            DWORD_PTR process_afm, system_afm;            
            if (!::GetProcessAffinityMask(process_handle, &process_afm, &system_afm))
            {
                show_failure_message("Getting Processor Afinity Mask Failed");
                break;
            }            
            
            // Find first available core 
            DWORD thisproces_afm = 0x01;
            while ((process_afm & 0x01) == 0)
            {
                process_afm >>= 1;
                thisproces_afm >>= 1;
            }
            
            if (!::SetProcessAffinityMask(process_handle, thisproces_afm))
            {
                show_failure_message("Setting Processor Afinity Mask Failed");
                break;
            }

            if (!::SetPriorityClass(process_handle, HIGH_PRIORITY_CLASS)) // REALTIME_PRIORITY_CLASS
            {
                show_failure_message("Setting Process Priority Class Failed");
                break;
            }

            if (!::SetThreadPriority(thread_handle, THREAD_PRIORITY_TIME_CRITICAL)) 
            {
                show_failure_message("Setting Thread Priority Class Failed");
                break;
            }

            result = true;
        }
        while (false);

        if (!result)
        {
            finalize_testing_mode();
        }

        ::SwitchToThread();

        return result;
    }

    static void finalize_testing_mode()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);  // Result ignored
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);   // Result ignores
        // Afinity mask remains unfinalized ...
    }
};


//////////////////////////////////////////////////////////////////////////

#ifdef USE_RTDSC_FOR_HIGHTIMER
__declspec(naked) unsigned __int64 get_tc() {
    __asm  {
        rdtsc;
        ret
    };
}
#endif // USE_RTDSC_FOR_HIGHTIMER    

class highres_timer
{
public:
    highres_timer()
    {
#ifdef USE_RTDSC_FOR_HIGHTIMER
#   pragma message ("error: Frequence inspection ss not implemented for rtdsc yet.")
#else
        LARGE_INTEGER frequency;
    
        if (!QueryPerformanceFrequency(&frequency))
        {
            throw std::runtime_error("Failed to get performance counter frequency");
        }

        m_frequency = frequency.QuadPart;
#endif // 
    }

#ifdef USE_RTDSC_FOR_HIGHTIMER    
    void start() { m_start_time = get_tc(); }
    void stop() { m_end_time = get_tc(); }
#else
    void start() { LARGE_INTEGER start_time; ::QueryPerformanceCounter(&start_time); m_start_time = start_time.QuadPart; }
    void stop() { LARGE_INTEGER end_time; ::QueryPerformanceCounter(&end_time); m_end_time = end_time.QuadPart; }
#endif // USE_RTDSC_FOR_HIGHTIMER
    
    unsigned long long get_duration()
    {
        LARGE_INTEGER time_taken;
        
        time_taken.QuadPart = m_end_time - m_start_time;
        time_taken.QuadPart *= 1000000;
        time_taken.QuadPart /= m_frequency;

        return time_taken.QuadPart;
    }

private:    
    unsigned long long m_start_time, m_end_time, m_frequency;
};

//////////////////////////////////////////////////////////////////////////

int main()
{
    void *code_address;

    if (!create_sample_memory_region(&code_address))
    {
        show_failure_message("Failed creating memory region");
        return EXIT_FAILURE;
    }

    if (!current_process::initialize_testing_mode())
    {
        show_failure_message("Failed to prepare program for testing");
        return EXIT_FAILURE;
    }

    int input = rand() % 1000;
    generate_sample_code(code_address, EXECSAMPLECODE_SIZEBYTES);
    sample_code_func_t sample_fn_ptr = (sample_code_func_t) code_address;
    int sample_result;

    highres_timer timer;
    
    timer.start();
        sample_result = sample_fn_ptr(100);
    timer.stop();

    current_process::finalize_testing_mode();

    unsigned long long time_taken = timer.get_duration();
    string result_message = format_results_message(sample_result, time_taken);    
    MessageBox(NULL, result_message.c_str(), "Results", 0);
}


//////////////////////////////////////////////////////////////////////////

// prolog ->>
//int sample_code(int input)
//{
//	int a, b = 0x12334, c = 0x3456, d = 0x7896, e, f;
//
//	// body ->>
//	a = input;
//	a = a * b; b = b * c; c = b * a; d = b * a * c;
//	a = a ^ b; b = b | (a ^ c); c = b >> a * 100;
//	e = a + b + c; a = e - a - b - c * 100; f = e / 1000 + 100 * a;
//  // epilog ->>
//	int result = a + b * c ;
//	return result;
//}

/*extern */
const uint8_t g_sample_code_program_prolog[] = 
{
	0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0xc7, 0x45,0xf8, 0x34, 0x23, 0x01, 0x00, 0xc7, 0x45, 0xf4,
	0x56, 0x34, 0x00, 0x00, 0xc7, 0x45, 0xec, 0x96,0x78, 0x00, 0x00, 0x8b, 0x45, 0x08, 0x89, 0x45,
	0xfc,
};

/*extern */
const uint8_t g_sample_code_program_epilog[] = 
{
	0x8, 0x45, 0xe8, 0x8b, 0xe5, 0x5d, 0xc3,
};


/*extern */
const uint8_t g_sample_program_code_body[] = 
{
	0x8b, 0x4d, 0xfc,0x0f, 0xaf, 0x4d, 0xf8, 0x89, 0x4d, 0xfc, 0x8b, 0x55, 0xf8, 0x0f, 0xaf, 0x55, 
	0xf4, 0x89, 0x55,0xf8, 0x8b, 0x45, 0xf8, 0x0f, 0xaf, 0x45, 0xfc, 0x89, 0x45, 0xf4, 0x8b, 0x4d, 
	0xf8, 0x0f, 0xaf,0x4d, 0xfc, 0x0f, 0xaf, 0x4d, 0xf4, 0x89, 0x4d, 0xec, 0x8b, 0x55, 0xfc, 0x33, 
	0x55, 0xf8, 0x89,0x55, 0xfc, 0x8b, 0x45, 0xfc, 0x33, 0x45, 0xf4, 0x0b, 0x45, 0xf8, 0x89, 0x45, 
	0xf8, 0x8b, 0x4d,0xfc, 0x6b, 0xc9, 0x64, 0x8b, 0x55, 0xf8, 0xd3, 0xfa, 0x89, 0x55, 0xf4, 0x8b, 
	0x45, 0xfc, 0x03,0x45, 0xf8, 0x03, 0x45, 0xf4, 0x89, 0x45, 0xf0, 0x8b, 0x4d, 0xf0, 0x2b, 0x4d, 
	0xfc, 0x2b, 0x4d,0xf8, 0x8b, 0x55, 0xf4, 0x6b, 0xd2, 0x64, 0x2b, 0xca, 0x89, 0x4d, 0xfc, 0x8b, 
	0x45, 0xf0, 0x99,0xb9, 0xe8, 0x03, 0x00, 0x00, 0xf7, 0xf9, 0x8b, 0x55, 0xfc, 0x6b, 0xd2, 0x64, 
	0x03, 0xc2, 0x89,0x45, 0xe4, 0x8b, 0x45, 0xf8, 0x0f, 0xaf, 0x45, 0xf4, 0x03, 0x45, 0xfc, 0x89, 
	0x45, 0xe8,
};

/*extern */
const size_t g_epilog_size = sizeof(g_sample_code_program_epilog);

/*extern */
const size_t g_body_size = sizeof(g_sample_program_code_body);

/*extern */
const size_t g_prolog_size = sizeof(g_sample_code_program_prolog);


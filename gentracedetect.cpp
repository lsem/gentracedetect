#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <cassert>
#include <cstdio>

using std::string;


//////////////////////////////////////////////////////////////////////////

#define EXECSAMPLECODE_ADDRESS		NULL
#define EXECSAMPLECODE_SIZEBYTES	(4096 * 10)


extern const uint8_t g_sample_code_program_prolog[];
extern const uint8_t g_sample_code_program_epilog[];
extern const uint8_t g_sample_program_code_body[];

extern const size_t g_epilog_size;
extern const size_t g_body_size;
extern const size_t g_prolog_size;


void show_failure_message(const string &user_message);
string format_results_message(int result_value, unsigned timeTaken);


//////////////////////////////////////////////////////////////////////////

bool create_sample_memory_region(void **p_out_region_address)
{
	bool result = false;

	HANDLE current_process_handle = GetCurrentProcess();
	const auto flags = MEM_COMMIT | MEM_RESERVE;
	const size_t region_size = EXECSAMPLECODE_SIZEBYTES;
	void *p_allocationAddress;
	
	do 
	{
		p_allocationAddress = VirtualAllocEx(current_process_handle, EXECSAMPLECODE_ADDRESS, region_size, flags, PAGE_EXECUTE_READWRITE);
		if (p_allocationAddress == NULL)
		{
			show_failure_message("Virtual Alloc Failed");
			break;
		}

		DWORD old_protect_flags;
		if (!VirtualProtectEx(current_process_handle, p_allocationAddress, region_size, PAGE_EXECUTE_READWRITE, &old_protect_flags))
		{
			show_failure_message("Virtual Protect Failed");
			break;
		}

		if (!FlushInstructionCache(current_process_handle, p_allocationAddress, region_size))
		{
			show_failure_message("Flushing instructions cache failed");
			break;
		}

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
	else
	{
		*p_out_region_address = p_allocationAddress;
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

		//std::cout << "generated nect chunk of body. New offset: " << offset << std::endl;
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
	string result;
	
	result += "Result: ";
	result += std::to_string(result_value);
	result += "\nTime Taken: ";
	result += std::to_string(timeTaken);

	return result;
}

typedef int (*sample_code_func)(int input);


//////////////////////////////////////////////////////////////////////////

int main()
{
	LARGE_INTEGER start_time, end_time, time_taken;
	LARGE_INTEGER frequency;

	int input = rand()% 1000;

	void *code_address;
	if (create_sample_memory_region(&code_address))
	{			
		generate_sample_code(code_address, EXECSAMPLECODE_SIZEBYTES);

		// detect current time
		QueryPerformanceFrequency(&frequency); 
		QueryPerformanceCounter(&start_time);
		
		int sample_result;
		// measured code
		{
			sample_code_func sample_fn_ptr = (sample_code_func) code_address;
			sample_result = sample_fn_ptr(100);

			QueryPerformanceCounter(&end_time);
			time_taken.QuadPart = end_time.QuadPart - start_time.QuadPart;
		}

		// detect current end time and calculate duration
		time_taken.QuadPart *= 1000000;
		time_taken.QuadPart /= frequency.QuadPart;

		string result_message = format_results_message(sample_result, time_taken.QuadPart);
		MessageBox(NULL, result_message.c_str(), "Results", 0);
	}
	else
	{
		MessageBox(NULL, "Failed to allocate executable memory region", "Failure", MB_ICONERROR);
	}	
}


//////////////////////////////////////////////////////////////////////////

//int sample_code(int input)
//{
//	int a, b = 0x12334, c = 0x3456, d = 0x7896, e, f;
//
//	a = input;
//	a = a * b; b = b * c; c = b * a; d = b * a * c;
//	a = a ^ b; b = b | (a ^ c); c = b >> a * 100;
//	e = a + b + c; a = e - a - b - c * 100; f = e / 1000 + 100 * a;
//
//	int result = a + b * c ;
//	return result;
//}

/*extern */
const uint8_t g_sample_code_program_prolog[] = 
{
	0x55, 
	0x8b, 
	0xec, 
	0x83,
	0xec, 
	0x1c, 
	0xc7, 
	0x45,
	0xf8, 
	0x34, 
	0x23, 
	0x01,
	0x00, 
	0xc7, 
	0x45, 
	0xf4,
	0x56, 
	0x34, 
	0x00, 
	0x00,
	0xc7, 
	0x45, 
	0xec, 
	0x96,
	0x78, 
	0x00, 
	0x00, 
	0x8b,
	0x45, 
	0x08, 
	0x89, 
	0x45,
	0xfc,
};

/*extern */
const uint8_t g_sample_code_program_epilog[] = 
{
	0x8,
	0x45, 
	0xe8, 
	0x8b, 
	0xe5,
	0x5d, 
	0xc3,
};


/*extern */
const uint8_t g_sample_program_code_body[] = 
{
	0x8b, 
	0x4d, 
	0xfc,
	0x0f, 
	0xaf, 
	0x4d, 
	0xf8,
	0x89, 
	0x4d, 
	0xfc, 
	0x8b,
	0x55, 
	0xf8, 
	0x0f, 
	0xaf,
	0x55, 
	0xf4, 
	0x89, 
	0x55,
	0xf8, 
	0x8b, 
	0x45, 
	0xf8,
	0x0f, 
	0xaf, 
	0x45, 
	0xfc,
	0x89, 
	0x45, 
	0xf4, 
	0x8b,
	0x4d, 
	0xf8, 
	0x0f, 
	0xaf,
	0x4d, 
	0xfc, 
	0x0f, 
	0xaf,
	0x4d, 
	0xf4, 
	0x89, 
	0x4d,
	0xec, 
	0x8b, 
	0x55, 
	0xfc,
	0x33, 
	0x55, 
	0xf8, 
	0x89,
	0x55, 
	0xfc, 
	0x8b, 
	0x45,
	0xfc, 
	0x33, 
	0x45, 
	0xf4,
	0x0b, 
	0x45, 
	0xf8, 
	0x89,
	0x45, 
	0xf8, 
	0x8b, 
	0x4d,
	0xfc, 
	0x6b, 
	0xc9, 
	0x64,
	0x8b, 
	0x55, 
	0xf8, 
	0xd3,
	0xfa, 
	0x89, 
	0x55, 
	0xf4,
	0x8b, 
	0x45, 
	0xfc, 
	0x03,
	0x45, 
	0xf8, 
	0x03, 
	0x45,
	0xf4, 
	0x89, 
	0x45, 
	0xf0,
	0x8b, 
	0x4d, 
	0xf0, 
	0x2b,
	0x4d, 
	0xfc, 
	0x2b, 
	0x4d,
	0xf8, 
	0x8b, 
	0x55, 
	0xf4,
	0x6b, 
	0xd2, 
	0x64, 
	0x2b,
	0xca, 
	0x89, 
	0x4d, 
	0xfc,
	0x8b, 
	0x45, 
	0xf0, 
	0x99,
	0xb9, 
	0xe8, 
	0x03, 
	0x00,
	0x00, 
	0xf7, 
	0xf9, 
	0x8b,
	0x55, 
	0xfc, 
	0x6b, 
	0xd2,
	0x64, 
	0x03, 
	0xc2, 
	0x89,
	0x45, 
	0xe4, 
	0x8b, 
	0x45,
	0xf8, 
	0x0f, 
	0xaf, 
	0x45,
	0xf4, 
	0x03, 
	0x45, 
	0xfc,
	0x89, 
	0x45, 
	0xe8,
};

/*extern */
const size_t g_epilog_size = sizeof(g_sample_code_program_epilog);

/*extern */
const size_t g_body_size = sizeof(g_sample_program_code_body);

/*extern */
const size_t g_prolog_size = sizeof(g_sample_code_program_prolog);


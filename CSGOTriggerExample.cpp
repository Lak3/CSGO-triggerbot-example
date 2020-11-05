// CSGOTriggerExample.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <chrono>
#include <thread>
#include <string>

constexpr int LIFE_ALIVE = 0;

//I dont know if this just makes things messier...
namespace G
{
	namespace Proc
	{
		DWORD m_dwPID = 0x0; //Process ID
		HANDLE m_hProc = 0; //Process handle
	}

	//https://github.com/frk1/hazedumper
	namespace Off
	{
		constexpr std::ptrdiff_t m_lifeState = 0x25F;
		constexpr std::ptrdiff_t m_iCrosshairId = 0xB3E4;
		constexpr std::ptrdiff_t m_iTeamNum = 0xF4;
		constexpr std::ptrdiff_t dwLocalPlayer = 0xD3DD14;
		constexpr std::ptrdiff_t dwEntityList = 0x4D5239C;
	}

	namespace Mem
	{
		//Nice function for reading memory
		//Is very easy to use just by using a template!
		template<class T>
		T Read(DWORD dwAddr)
		{
			T cData;
			ReadProcessMemory(G::Proc::m_hProc, reinterpret_cast<LPCVOID>(dwAddr), &cData, sizeof(T), 0);
			return cData;
		}
	}
}

//Simple working function which opens a handle and grabs the process ID of the wanted process
//Returns true is succeeded, false if failed.
bool GetProcess(const wchar_t* szName)
{
	if (auto hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0))
	{
		PROCESSENTRY32 ProcEntry;
		ProcEntry.dwSize = sizeof(ProcEntry);

		while (Process32Next(hProcess, &ProcEntry))
		{
			if (!wcscmp(ProcEntry.szExeFile, szName))
			{
				CloseHandle(hProcess);

				G::Proc::m_dwPID = ProcEntry.th32ProcessID;
				G::Proc::m_hProc = OpenProcess(0x38, 0, G::Proc::m_dwPID);

				return true;
			}
		}

		CloseHandle(hProcess);
	}

	return false;
}

//Simple working function which looks for a wanted module in the initialized process
//Returns base addr (DWORD) if succeeded, NULL (0x0) in case it failed
DWORD GetModule(const wchar_t* szModule)
{
	if (auto hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, G::Proc::m_dwPID))
	{
		MODULEENTRY32 ModEntry;
		ModEntry.dwSize = sizeof(ModEntry);

		while (Module32Next(hModule, &ModEntry))
		{
			if (!wcscmp(ModEntry.szModule, szModule))
			{
				CloseHandle(hModule);
				return reinterpret_cast<DWORD>(ModEntry.modBaseAddr);
			}
		}

		CloseHandle(hModule);
	}

	return 0x0;
}

int main()
{
	//Set the console title to what this "project" is
	//An example on how to write a working triggerbot externally for CS:GO
	//This method also works on many other SE games, but can be very inaccurate.
	SetConsoleTitleW(L"CSGO external triggerbot example");

	//Grab proc handle and ID of the CSGO process
	if (GetProcess(L"csgo.exe"))
	{
		//Grab client baseaddr
		if (const auto dwClient = GetModule(L"client.dll"))
		{
			//If we here we got all we need, so print this to inform that we running!
			wprintf(L"Running!\n");

			//Infinite loop, which breaks when F11 is pressed
			while (!(GetAsyncKeyState(VK_F11) & 0x01))
			{
				//Read local
				if (const auto& dwLocal = G::Mem::Read<DWORD>(dwClient + G::Off::dwLocalPlayer))
				{
					//No point looping trough the players in triggerbot
					//As we can just see if we manage to read a valid entity with our corrhair ID
					//As that represents the index of the player that is under our crosshair

					//Read the crosshair ID from localplayer
					const auto nCrossID = G::Mem::Read<int>(dwLocal + G::Off::m_iCrosshairId);
				
					//Simple way is to check if the ID is in bounds of the default playerslots
					//NOTE: Can go up to 128
					//More proper way would be to check the entity's class ID, so you can easily target 
					//Other entities too like chicken
					if (nCrossID <= 64 && nCrossID >= 0)
					{
						//Then we proceed to see if we can read a valid entity with the current crosshair ID
						if (const auto& dwEntity = G::Mem::Read<DWORD>(dwClient + G::Off::dwEntityList + (nCrossID * 0x10)))
						{
							//I believe we should not even be able to get here with the local entity
							//So we are not going to check for (dwEntity == dwLocal)

							//Check if entity is alive, we don't want entities that are already dead
							if (G::Mem::Read<byte>(dwEntity + G::Off::m_lifeState) == LIFE_ALIVE)
							{
								//We only want a player that is NOT our teammate
								//So check that the entity's team number is NOT the same as local's
								if (G::Mem::Read<int>(dwEntity + G::Off::m_iTeamNum) != G::Mem::Read<int>(dwLocal + G::Off::m_iTeamNum))
								{
									//Our hardcoded triggerbot "bind", MOUSE5 button (0x06)
									//If it's held then we want to shoot 
									if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000)
									{
										//Ofc you can use "SendInput" or whatever here
										//I just felt that "mouse_event" is just what I need here

										//Send left button down
										mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);

										//Sleep, no use really I guess you could randomize this delay
										std::this_thread::sleep_for(std::chrono::milliseconds(10));

										//Send left button up back up
										mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
									}
								}
							}
						}
					}
				}

				//We do not want to loop the run full time, eats a lot of performance
				//Sleeping 1ms will ease the performance a bit, and is fast enough to still work as intended
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		else //A little output in case something goes wrong with getting the "client.dll" base addr
		{
			wprintf(L"Not able to access client.dll!\n");
		}
	}
	else //A little output in case we were not able to find "csgo.exe"
	{
		wprintf(L"Did not find csgo.exe!\n");
	}

	//If we here that means something failed, or we pressed the key which breaks the loop
	//Anyhow we want to shut down the process

	//Close the handle when we shutting down, if it was even opened in the first place
	if (G::Proc::m_hProc)
		CloseHandle(G::Proc::m_hProc);

	//Inform the user about it
	wprintf(L"Quitted!\n");
	_wsystem(L"pause"); //"pause" gets the job done for this example

	//Our mandatory return
	return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

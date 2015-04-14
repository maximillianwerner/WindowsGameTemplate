/*=============================
  $FILE:GameTemplate.cpp $
  $DATE: 3/13/15 $
  $AUTHOR: Maximillian Werner $
  ============================*/
  
#include <windows.h>
#include <stdint.h>
#include <xinput.h> //for controller support
#include <dsound.h> //for sound
#include <math.h>

#define internal static //as in internal function
#define global_variable static
#define Pi32 3.14159265359f

struct OffscreenBuffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch; //Pitch is what needs to be added to the Row pointer to reach the next row of pixels
	int BytesPerPixel = 4;
};

struct WindowDimension
{
	int Width;
	int Height;
};

struct SoundTestStruct
{
	//Sound Test Variables
	int SamplesPerSecond;
	int ToneHz; //About a mid C
	int16_t ToneVolume;
	uint32_t RunningSampleIndex;
	int WavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
};

global_variable bool RUNNING; 
global_variable OffscreenBuffer RENDER_BUFFER;
global_variable LPDIRECTSOUNDBUFFER SOUND_BUFFER;


//I manually load the xinput.lib and Dsound.lib so the program wont crash if they don't work

//Manually load and support GetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(0);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//Manually load and support SetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(0);
}	
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

//Initialize the dll's and necessary functions of XInput API
internal void InitXInput()
{
	//Try loading the dll everybosy using Windows 7 has	
	HMODULE XInputLibrary = LoadLibrary("xinput1_3.dll");
	if(!XInputLibrary)
		//This is for the Windows 8 users
		XInputLibrary = LoadLibrary("xinput1_4.dll");
	if(XInputLibrary)
	{
		//GetProcAddress gets the address to a procedure
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState)
			XInputGetState = XInputGetStateStub;
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState)
			XInputSetState = XInputSetStateStub;
	}
}

//Step by step process for initializing and creating DirectSound buffers
internal void InitDirectSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
	//Steps for getting DirectSound functioning: 
	//1. Load library 
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	
	if(DSoundLibrary)
	{	
		//2. Get a dsound object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		
		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			//initialize the members of the struct
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2; //at least stereo sound
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			
			//3. Create a primary buffer for setting the sound mode on the sound card 
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if(SUCCEEDED(Error))
					{
						//The primary buffer has now been created and has the correct format
						OutputDebugStringA("PRIMARY_BUFFER SET\n");
					}						
				}				
			}
			
			//4. Create secondary buffer to write to 
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			
			HRESULT ErrorTwo = DirectSound->CreateSoundBuffer(&BufferDescription, &SOUND_BUFFER, 0);
			if(SUCCEEDED(ErrorTwo))
			{
				//Secondary buffer was created successfully
				OutputDebugStringA("SECONDARY_BUFFER SET\n");
			}
		}	
	}	
}	

//Helper function: gets the dimensions of the window to blit to
internal WindowDimension GetWindowDimension(HWND Window)
{
	WindowDimension Current;
	RECT ClientRect;
			
	//This gets the portion of the window that you will actually be using
	GetClientRect(Window, &ClientRect);
	
	Current.Height =  ClientRect.bottom - ClientRect.top;
	Current.Width = ClientRect.right - ClientRect.left;
	return Current;
}

//A test function that renders certain RGB colors by setting them pixel by pixel
internal void RGBRenderer(OffscreenBuffer *Buffer, int xOffset, int yOffset)
{		
	uint8_t *Row = (uint8_t *)Buffer->Memory; 
	
	for(int y = 0; y < Buffer->Height; ++y)
	{
		uint32_t *Pixel = (uint32_t *)Row;
		for(int x = 0; x < Buffer->Width; ++x)
		{
			uint8_t Blue = (x + xOffset);
			uint8_t Green = (y + yOffset);
			
			*Pixel++ = ((Blue << 16) | Green);
			//*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}	
}

//DIB = Device Independent Bitmap
//Redraw the buffer to the window if the window is resized
internal void RedrawDIBSection(OffscreenBuffer *Buffer, int Width, int Height)
{
	//Clear the buffer initially
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}	
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	
	//set the info required
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; //negative height means top down DIB
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	//the rest are cleared to zero thanks to the global variable
	
	int BitmapMemorySize  = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	
	//VirtualAlloc does pretty much what you'd think and gives us the memory we need
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

//Copy the buffer to the window
internal void Win32UpdateWindow(OffscreenBuffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{	
	//Scale the bitmap to the window (a rectangle to rectangle copy)
	StretchDIBits(DeviceContext,
				  0, 0, WindowWidth, WindowHeight,
				  0, 0, Buffer->Width, Buffer->Height,
				  Buffer->Memory,
				  &Buffer->Info,
				  DIB_RGB_COLORS,
				  SRCCOPY);

}

//The message handler for the messages Windows sends
LRESULT CALLBACK 
WinMainProcCallBack(
					HWND Window,
					UINT Message,
					WPARAM wParam,
					LPARAM lParam)
{
	LRESULT Result = 0;
	
	switch(Message)
	{	
		//if the window is closed by the system
		case WM_DESTROY:
		{
			RUNNING = false;
		} break;
		
		//if the user closes the window
		case WM_CLOSE:
		{
			RUNNING = false;
		} break;
		
		//if this is the active window
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACVTIVATEAPP\n");
		} break;
		
		//keyboard support
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32_t VKCode = wParam;
			bool WasDown = ((lParam & (1 << 30)) != 0); //You will get either bit 0 or 30 when oring, so the != makes it so its either a 0 or 1 for the bool type
			bool IsDown = ((lParam & (1 << 31)) == 0);
			if(WasDown != IsDown)
			{
				if(VKCode == 'W')
				{
				}
				else if(VKCode == 'A')
				{
				}
				else if(VKCode == 'S')
				{
				}	
				else if(VKCode == 'D')
				{
				}	
				else if(VKCode == 'Q')
				{
				}	
				else if(VKCode == 'E')
				{
				}
				else if(VKCode == VK_UP)
				{
				}			
				else if(VKCode == VK_DOWN)
				{
				}			
				else if(VKCode == VK_LEFT)
				{
				}
				else if(VKCode == VK_RIGHT)
				{
				}						
				else if(VKCode == VK_ESCAPE)
				{
				}						
				else if(VKCode == VK_SPACE)
				{
				}
			}
			
			//Allow closing the window with Alt-F4 because we are handling SYSKEYUP/DOWN
			bool AltWasPressed = ((lParam & (1 << 29)) != 0);
			if((VKCode == VK_F4) && AltWasPressed)
			{
				RUNNING = false;
			}	
		} break;
		
		//to paint to our window
		case WM_PAINT:
		{
			//the PAINTSTRUCT variable that will be painted
			PAINTSTRUCT Paint;
			
			//marks the beginning of the painting
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			WindowDimension Current = GetWindowDimension(Window);
			Win32UpdateWindow(&RENDER_BUFFER, DeviceContext, Current.Width, Current.Height);
			
			//marks the end of the painting
			EndPaint(Window, &Paint);
		} break;	
		
		default:
		{
			//for all other non specified cases use the following function to handle it
			Result = DefWindowProc(Window, Message, wParam, lParam);
		} break;
	}
	
	return(Result);
}						


//Write a sine wave to the second sound buffer
internal void Win32FillSoundBuffer(SoundTestStruct *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
			
	//Lock the buffer to write to it
	if(SUCCEEDED(SOUND_BUFFER->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{	
		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16_t *SampleOut = (int16_t *)Region1;
			
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			float t = 2.0f*Pi32*(float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
							
			++SoundOutput->RunningSampleIndex;
		}							
		
		SampleOut = (int16_t *)Region2;
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region2Size; ++SampleIndex)
		{
			float t = 2.0f*Pi32*(float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
							
			++SoundOutput->RunningSampleIndex;
		}
						
		SOUND_BUFFER->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

//enter windows  
int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCode)
{
	WNDCLASSA WindowClass = {};
	
	RedrawDIBSection(&RENDER_BUFFER, 1280, 720);
	
	//Window properties (a bit field)
	//REDRAW flags ensures the entire screen is redrawn when a change occurs 
	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    
	//pointer to our main window function
	WindowClass.lpfnWndProc = WinMainProcCallBack;
    
	//the instance of code currently running
	WindowClass.hInstance = Instance;
    
	//sets up an icon
	//Window.hIcon;
   
	//Give your window class a proper name
    WindowClass.lpszClassName = "TestWindowClass";
	
	InitXInput();
	
	//this will actually register and then spawn our desired window
	if(RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
								//we are not currently going to customize the window  
								/* DWORD ExtendedStyle = */ 0,
								
								//our window class' name
								/*LPCTSTR lpClassName = */ WindowClass.lpszClassName,
								/*LPCTSTR lpWindowName = */ "WindowsGameTemplate",
								
								//standardizes the window to have basic functionality and make it visible
								/*DWORD dwStyle = */ WS_OVERLAPPEDWINDOW|WS_VISIBLE,
								
								//use standard sizing of the window
								/*int x = */ CW_USEDEFAULT,
								/*int y = */ CW_USEDEFAULT,
								/*int nWidth = */ CW_USEDEFAULT,
								/*int nHeight = */ CW_USEDEFAULT,
								
								//we don't want nested windows so set to 0
								/*HWND hWndParent = */ 0,
								
								//no menu plz
								/*HMENU hMenu = */ 0,
								
								//set our instance we've created
								/*HINSTANCE hInstance =*/ Instance,
								
								//no need to pass anything to our window
								/* LPVOID lpParam = */ 0);
		if(Window)
		{
			//This is where we should put the DC
			HDC DeviceContext = GetDC(Window);
			
			//Graphics Test Variables
			int xOffset = 0;
			int yOffset = 0;
			
			SoundTestStruct SoundOutput = {};
				
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.ToneHz = 262; //About a mid C
			SoundOutput.ToneVolume = 3000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
			SoundOutput.BytesPerSample = sizeof(int16_t)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			
			InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
			SOUND_BUFFER->Play(0, 0, DSBPLAY_LOOPING);
			
			
			RUNNING = true;
			while(RUNNING)
			{
				MSG Message;
				while(PeekMessage(&Message, 0, 0, 0,  PM_REMOVE))
				{
					if(Message.message == WM_QUIT)
						RUNNING = false;
					
					//prepare received message
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}
				for (DWORD i=0; i < XUSER_MAX_COUNT; ++i)
				{
					XINPUT_STATE ControllerState;
					if(XInputGetState(i, &ControllerState) == ERROR_SUCCESS)
					{
						//controller is connected and working properly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
						
						uint16_t StickX = Pad->sThumbLX;
						uint16_t StickY = Pad->sThumbLY;
						
						//Button test
						if(AButton)
						{
							yOffset += 2;
						}	
					}
				}
				
				//Vibration test
				XINPUT_VIBRATION GoodVibes;
				GoodVibes.wLeftMotorSpeed = 60000;
				GoodVibes.wRightMotorSpeed = 60000;
				XInputSetState(0, &GoodVibes);
				
				RGBRenderer(&RENDER_BUFFER, xOffset, yOffset);
				
				//DirectSound Test
				DWORD PlayCursor;  //Where the sound card should be with the processing
				DWORD WriteCursor;  //Where the buffer actually is with the processing (should be ahead of the PlayCursor)
				if(SUCCEEDED(SOUND_BUFFER->GetCurrentPosition(&PlayCursor, &WriteCursor)))
				{	
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					DWORD BytesToWrite;
					
					//On start up
					if(ByteToLock == PlayCursor)
					{
						BytesToWrite = 0;
					}	
					
					//ByteToWrite is ahead of the PlayCursor
					else if(ByteToLock > PlayCursor)
					{
						BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
						BytesToWrite += PlayCursor;
					}
					
					//BytesToWrite is behind the play cursor
					else
					{
						BytesToWrite = PlayCursor - ByteToLock;
					}
					
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
				}
				
				WindowDimension Current = GetWindowDimension(Window);
				Win32UpdateWindow(&RENDER_BUFFER, DeviceContext, Current.Width, Current.Height);
				xOffset++;
			}	
		}
	}
	return(0);
}	
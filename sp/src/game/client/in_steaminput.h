//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Community integration of Steam Input on Source SDK 2013.
//
// Author: Blixibon
//
// $NoKeywords: $
//=============================================================================//

#ifndef IN_STEAMINPUT_H
#define IN_STEAMINPUT_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------

#define IsDeck() (CommandLine()->CheckParm("-deck") != 0)

//-----------------------------------------------------------------------------

void UTIL_ReplaceKeyBindingsWithGlyphs( const wchar_t *inbuf, int inbufsizebytes, OUT_Z_BYTECAP( outbufsizebytes ) wchar_t *outbuf, int outbufsizebytes, vgui::HFont &hFont, vgui::IScheme *pScheme );

//-----------------------------------------------------------------------------

typedef uint64 InputActionSetHandle_t;
typedef uint64 InputDigitalActionHandle_t;
typedef uint64 InputAnalogActionHandle_t;

struct InputDigitalActionBind_t
{
	InputDigitalActionHandle_t handle;
	bool bDown;

	virtual void OnDown() { ; }
	virtual void OnUp() { ; }
};

struct InputDigitalActionCommandBind_t : public InputDigitalActionBind_t
{
	InputDigitalActionCommandBind_t( const char *_pszActionName, const char *_pszBindCommand )
	{
		pszActionName = _pszActionName;
		pszBindCommand = _pszBindCommand;
	}

	const char *pszActionName;
	const char *pszBindCommand;

	virtual void OnDown()
	{
		engine->ClientCmd_Unrestricted( VarArgs( "%s\n", pszBindCommand ) );
	}

	virtual void OnUp()
	{
		if (pszBindCommand[0] == '+')
		{
			// Unpress the bind
			engine->ClientCmd_Unrestricted( VarArgs( "-%s\n", pszBindCommand+1 ) );
		}
	}
};

abstract_class ISource2013SteamInput
{
public:

	virtual void Init() = 0;

	virtual void LevelInitPreEntity() = 0;

	virtual void RunFrame() = 0;
	
	// "Enabled" just means a controller is being used
	virtual bool IsEnabled() = 0;

	//-----------------------------------------------------------------------------
	
	virtual const char *GetControllerName() = 0;

	//-----------------------------------------------------------------------------

	virtual bool UsingJoysticks() = 0;
	virtual void GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
		bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw ) = 0;

	virtual void SetRumble( float fLeftMotor, float fRightMotor, int userId = INVALID_USER_ID ) = 0;
	virtual void StopRumble( void ) = 0;

	//-------------------------------------------

	virtual void SetLEDColor( byte r, byte g, byte b ) = 0;
	virtual void ResetLEDColor() = 0;

	//-----------------------------------------------------------------------------

	virtual bool UseGlyphs() = 0;
	virtual bool TintGlyphs() = 0;
	virtual void GetGlyphFontForCommand( const char *pszCommand, char *szChars, int szCharsSize, vgui::HFont &hFont, vgui::IScheme *pScheme ) = 0;
	virtual void GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char*> &szStringList ) = 0;

	virtual const char *GetGlyphPNGForCommand( const char *pszCommand ) = 0;
	virtual const char *GetGlyphSVGForCommand( const char *pszCommand ) = 0;

	//-----------------------------------------------------------------------------

	virtual void RemapHudHint( const char **pszInputHint ) = 0;

private:
};

extern ISource2013SteamInput *g_pSteamInput;

#endif // IN_MAIN_H

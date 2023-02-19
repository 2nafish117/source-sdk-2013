//=============================================================================//
//
// Purpose: Community integration of Steam Input on Source SDK 2013.
//
// Author: Blixibon
//
// $NoKeywords: $
//=============================================================================//

#include "expanded_steam/isteaminput.h"

#include "inputsystem/iinputsystem.h"
#include "GameUI/IGameUI.h"
#include "IGameUIFuncs.h"
#include "ienginevgui.h"
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include "steam/isteaminput.h"
#include "steam/isteamutils.h"
#include "icommandline.h"
#include "cdll_int.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "filesystem.h"

#include "libpng/png.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------

IVEngineClient *g_pEngineClient = NULL;
IEngineVGui *g_pEngineVGui = NULL;

// Copied from cdll_util.cpp
char *SteamInput_VarArgs( const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof( string ), format,argptr);
	va_end (argptr);

	return string;	
}

//-------------------------------------------

#define USE_HL2_INSTALLATION 1 // This attempts to obtain HL2's action manifest from the user's own HL2 or Portal installations
#define MENU_ACTIONS_ARE_BINDS 1

InputActionSetHandle_t g_AS_GameControls;
InputActionSetHandle_t g_AS_VehicleControls;
InputActionSetHandle_t g_AS_MenuControls;

//-------------------------------------------

struct InputDigitalActionCommandBind_t : public InputDigitalActionBind_t
{
	InputDigitalActionCommandBind_t()
	{
		pszActionName = NULL;
		pszBindCommand = NULL;
	}

	InputDigitalActionCommandBind_t( const char *_pszActionName, const char *_pszBindCommand )
	{
		pszActionName = _pszActionName;
		pszBindCommand = _pszBindCommand;
	}

	const char *pszActionName;
	const char *pszBindCommand;

	void OnDown() override
	{
		g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "%s\n", pszBindCommand ) );
	}

	void OnUp() override
	{
		if (pszBindCommand[0] == '+')
		{
			// Unpress the bind
			g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "-%s\n", pszBindCommand+1 ) );
		}
	}
};

static CUtlVector<InputDigitalActionCommandBind_t> g_DigitalActionBinds;

// Stores strings parsed for action binds
static CUtlStringList g_DigitalActionBindNames;

// Special cases from the above
InputDigitalActionBind_t *g_DAB_Zoom;
InputDigitalActionBind_t *g_DAB_Brake;
InputDigitalActionBind_t *g_DAB_Duck;
InputDigitalActionBind_t *g_DAB_MenuPause;
InputDigitalActionBind_t *g_DAB_Toggle_Zoom;
InputDigitalActionBind_t *g_DAB_Toggle_Duck;

//-------------------------------------------

InputAnalogActionHandle_t g_AA_Move;
InputAnalogActionHandle_t g_AA_Camera;
InputAnalogActionHandle_t g_AA_JoystickCamera;

//-------------------------------------------

InputAnalogActionHandle_t g_AA_Steer;
InputAnalogActionHandle_t g_AA_Accelerate;
InputAnalogActionHandle_t g_AA_Brake;

//-------------------------------------------

#if MENU_ACTIONS_ARE_BINDS
InputDigitalActionBind_t g_DAB_MenuUp;
InputDigitalActionBind_t g_DAB_MenuDown;
InputDigitalActionBind_t g_DAB_MenuLeft;
InputDigitalActionBind_t g_DAB_MenuRight;
InputDigitalActionBind_t g_DAB_MenuSelect;
InputDigitalActionBind_t g_DAB_MenuCancel;
InputDigitalActionBind_t g_DAB_MenuLB;
InputDigitalActionBind_t g_DAB_MenuRB;
InputDigitalActionBind_t g_DAB_MenuX;
InputDigitalActionBind_t g_DAB_MenuY;
#else
InputDigitalActionHandle_t g_DA_MenuUp;
InputDigitalActionHandle_t g_DA_MenuDown;
InputDigitalActionHandle_t g_DA_MenuLeft;
InputDigitalActionHandle_t g_DA_MenuRight;
InputDigitalActionHandle_t g_DA_MenuSelect;
InputDigitalActionHandle_t g_DA_MenuCancel;
InputDigitalActionHandle_t g_DA_MenuLB;
InputDigitalActionHandle_t g_DA_MenuRB;
#endif

InputAnalogActionHandle_t g_AA_Mouse;

//-------------------------------------------

CON_COMMAND( pause_menu, "Shortcut to toggle pause menu" )
{
	if (g_pEngineVGui->IsGameUIVisible())
	{
		g_pEngineClient->ClientCmd_Unrestricted( "gameui_hide" );
	}
	else
	{
		g_pEngineClient->ClientCmd_Unrestricted( "gameui_activate" );
	}
}

//-------------------------------------------

static ConVar si_current_cfg( "si_current_cfg", "0", FCVAR_ARCHIVE, "Steam Input's current controller." );

static ConVar si_force_glyph_controller( "si_force_glyph_controller", "-1", FCVAR_NONE, "Forces glyphs to translate to the specified ESteamInputType." );
static ConVar si_default_glyph_controller( "si_default_glyph_controller", "0", FCVAR_NONE, "The default ESteamInputType to use when a controller's glyphs aren't available." );

static ConVar si_use_glyphs( "si_use_glyphs", "1", FCVAR_NONE, "Whether or not to use controller glyphs for hints." );

static ConVar si_enable_rumble( "si_enable_rumble", "1", FCVAR_NONE, "Enables controller rumble triggering vibration events in Steam Input. If disabled, rumble is directed back to the input system as before." );

static ConVar si_hintremap( "si_hintremap", "1", FCVAR_NONE, "Enables the hint remap system, which remaps HUD hints based on the current controller configuration." );

static ConVar si_print_action_set( "si_print_action_set", "0" );
static ConVar si_print_joy_src( "si_print_joy_src", "0" );
static ConVar si_print_rumble( "si_print_rumble", "0" );
static ConVar si_print_hintremap( "si_print_hintremap", "0" );

//-------------------------------------------

class CSource2013SteamInput : public ISource2013SteamInput
{
public:

	CSource2013SteamInput()
	{

	}

	~CSource2013SteamInput();

	void Initialize( CreateInterfaceFn factory ) override;

	void InitSteamInput() override;
	void InitActionManifest();

	void LevelInitPreEntity() override;

	void Shutdown() override;

	void RunFrame( ActionSet_t &iActionSet ) override;
	
	bool IsEnabled() override;

	//-------------------------------------------

	bool IsSteamRunningOnSteamDeck() override;
	void SetGamepadUI( bool bToggle ) override;
	
	InputHandle_t GetActiveController() override;
	int GetConnectedControllers( InputHandle_t *nOutHandles ) override;

	const char *GetControllerName( InputHandle_t nController ) override;
	int GetControllerType( InputHandle_t nController ) override;

	bool ShowBindingPanel( InputHandle_t nController ) override;

	//-------------------------------------------

	void LoadActionBinds( const char *pszFileName );
	InputDigitalActionCommandBind_t *FindActionBind( const char *pszActionName );

	bool TestActions( int iActionSet, InputHandle_t nController );

	void TestDigitalActionBind( InputHandle_t nController, InputDigitalActionBind_t &DigitalAction, bool &bActiveInput );
#if MENU_ACTIONS_ARE_BINDS
	void PressKeyFromDigitalActionHandle( InputHandle_t nController, InputDigitalActionBind_t &nHandle, ButtonCode_t nKey, bool &bActiveInput );
#else
	void PressKeyFromDigitalActionHandle( InputHandle_t nController, InputDigitalActionHandle_t nHandle, ButtonCode_t nKey, bool &bActiveInput );
#endif

	bool UsingJoysticks() override;
	void GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
		bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw ) override;

	void SetRumble( InputHandle_t nController, float fLeftMotor, float fRightMotor, int userId = INVALID_USER_ID ) override;
	void StopRumble() override;

	//-------------------------------------------
	
	void SetLEDColor( InputHandle_t nController, byte r, byte g, byte b ) override;
	void ResetLEDColor( InputHandle_t nController ) override;

	//-------------------------------------------

	int FindDigitalActionsForCommand( const char *pszCommand, InputDigitalActionHandle_t *pHandles );
	int FindAnalogActionsForCommand( const char *pszCommand, InputAnalogActionHandle_t *pHandles );
	void GetInputActionOriginsForCommand( const char *pszCommand, CUtlVector<EInputActionOrigin> &actionOrigins, int iActionSetOverride = -1 );

	void GetGlyphPNGsForCommand( CUtlVector<const char*> &szImgList, const char *pszCommand, int &iSize, int iStyle ) override;
	void GetGlyphSVGsForCommand( CUtlVector<const char*> &szImgList, const char *pszCommand ) override;

	virtual bool UseGlyphs() override { return si_use_glyphs.GetBool(); };
	void GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char *> &szStringList, int iActionSet = -1 ) override;

	bool GetPNGBufferFromFile( const char *filename, CUtlMemory< byte > &buffer ) override;

	//-------------------------------------------

	void LoadHintRemap( const char *pszFileName );
	void RemapHudHint( const char **pszInputHint ) override;

private:
	const char *IdentifyControllerParam( ESteamInputType inputType );

	void InputDeviceConnected( InputHandle_t nDeviceHandle );
	void InputDeviceDisconnected( InputHandle_t nDeviceHandle );
	void InputDeviceChanged( InputHandle_t nOldHandle, InputHandle_t nNewHandle );
	void DeckConnected( InputHandle_t nDeviceHandle );

	//-------------------------------------------

	// Provides a description for the specified action using GetStringForActionOrigin()
	const char *LookupDescriptionForActionOrigin( EInputActionOrigin eAction );

	//-------------------------------------------

	bool m_bEnabled;

	// Handle to the active controller (may change depending on last input)
	InputHandle_t m_nControllerHandle = 0;

	InputAnalogActionData_t m_analogMoveData, m_analogCameraData;

	InputActionSetHandle_t m_iLastActionSet;

	bool m_bIsGamepadUI;

	//-------------------------------------------

	enum
	{
		HINTREMAPCOND_NONE,
		HINTREMAPCOND_INPUT_TYPE,		// Only for the specified type of controller
		HINTREMAPCOND_ACTION_BOUND,		// Only if the specified action is bound
	};

	struct HintRemapCondition_t
	{
		int iType;
		bool bNot;
		char szParam[32];
	};

	struct HintRemap_t
	{
		const char *pszOldHint;
		const char *pszNewHint;

		CUtlVector<HintRemapCondition_t> nRemapConds;
	};

	CUtlVector< HintRemap_t >	m_HintRemaps;
};

//EXPOSE_SINGLE_INTERFACE( CSource2013SteamInput, ISource2013SteamInput, SOURCE2013STEAMINPUT_INTERFACE_VERSION );

// TODO: Replace with proper singleton interface in the future
ISource2013SteamInput *CreateSource2013SteamInput()
{
	static CSource2013SteamInput g_SteamInput;
	return &g_SteamInput;
}

//-------------------------------------------

CSource2013SteamInput::~CSource2013SteamInput()
{
	SteamInput()->Shutdown();
}

//-------------------------------------------

void CSource2013SteamInput::Initialize( CreateInterfaceFn factory )
{
	g_pEngineClient = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
	g_pEngineVGui = (IEngineVGui *)factory( VENGINE_VGUI_VERSION, NULL );
}

//-------------------------------------------

void CSource2013SteamInput::InitSteamInput()
{
	bool bInit = false;

	if (CommandLine()->CheckParm( "-nosteamcontroller" ) == 0 && SteamUtils()->IsOverlayEnabled())
	{
		// Do this before initializing SteamInput()
		InitActionManifest();

		bInit = SteamInput()->Init( true );
	}

	if (!bInit)
	{
		Msg( "SteamInput didn't initialize\n" );

		if (si_current_cfg.GetString()[0] != '0')
		{
			Msg("Reverting leftover Steam Input cvars\n");
			g_pEngineClient->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );
			g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_uninput_%s.cfg", si_current_cfg.GetString() ) );
		}

		return;
	}

	Msg( "SteamInput initialized\n" );

	m_bEnabled = false;
	//SteamInput()->EnableDeviceCallbacks();

	g_AS_GameControls		= SteamInput()->GetActionSetHandle( "GameControls" );
	g_AS_VehicleControls	= SteamInput()->GetActionSetHandle( "VehicleControls" );
	g_AS_MenuControls		= SteamInput()->GetActionSetHandle( "MenuControls" );

	SteamInput()->ActivateActionSet( m_nControllerHandle, g_AS_GameControls );

	// Load the KV
	LoadActionBinds( "scripts/steaminput_actionbinds.txt" );

	if (g_DigitalActionBinds.Count())
	{
		Msg( "SteamInput has no action binds, will not run" );
		return;
	}

	// Fill out special cases
	g_DAB_Zoom				= FindActionBind("zoom");
	g_DAB_Brake				= FindActionBind("jump");
	g_DAB_Duck				= FindActionBind("duck");
	g_DAB_MenuPause			= FindActionBind("pause_menu");
	g_DAB_Toggle_Zoom		= FindActionBind("toggle_zoom");
	g_DAB_Toggle_Duck		= FindActionBind("toggle_duck");

	g_AA_Move				= SteamInput()->GetAnalogActionHandle( "Move" );
	g_AA_Camera				= SteamInput()->GetAnalogActionHandle( "Camera" );
	g_AA_JoystickCamera		= SteamInput()->GetAnalogActionHandle( "JoystickCamera" );
	g_AA_Steer				= SteamInput()->GetAnalogActionHandle( "Steer" );
	g_AA_Accelerate			= SteamInput()->GetAnalogActionHandle( "Accelerate" );
	g_AA_Brake				= SteamInput()->GetAnalogActionHandle( "Brake" );
	g_AA_Mouse				= SteamInput()->GetAnalogActionHandle( "Mouse" );

#if MENU_ACTIONS_ARE_BINDS
	g_DAB_MenuUp.handle		= SteamInput()->GetDigitalActionHandle( "menu_up" );
	g_DAB_MenuDown.handle	= SteamInput()->GetDigitalActionHandle( "menu_down" );
	g_DAB_MenuLeft.handle	= SteamInput()->GetDigitalActionHandle( "menu_left" );
	g_DAB_MenuRight.handle	= SteamInput()->GetDigitalActionHandle( "menu_right" );
	g_DAB_MenuSelect.handle	= SteamInput()->GetDigitalActionHandle( "menu_select" );
	g_DAB_MenuCancel.handle	= SteamInput()->GetDigitalActionHandle( "menu_cancel" );
	g_DAB_MenuX.handle		= SteamInput()->GetDigitalActionHandle( "menu_x" );
	g_DAB_MenuY.handle		= SteamInput()->GetDigitalActionHandle( "menu_y" );
	g_DAB_MenuLB.handle		= SteamInput()->GetDigitalActionHandle( "menu_lb" );
	g_DAB_MenuRB.handle		= SteamInput()->GetDigitalActionHandle( "menu_rb" );
#else
	g_DA_MenuUp				= SteamInput()->GetDigitalActionHandle( "menu_up" );
	g_DA_MenuDown			= SteamInput()->GetDigitalActionHandle( "menu_down" );
	g_DA_MenuLeft			= SteamInput()->GetDigitalActionHandle( "menu_left" );
	g_DA_MenuRight			= SteamInput()->GetDigitalActionHandle( "menu_right" );
	g_DA_MenuSelect			= SteamInput()->GetDigitalActionHandle( "menu_select" );
	g_DA_MenuCancel			= SteamInput()->GetDigitalActionHandle( "menu_cancel" );
	g_DA_MenuLB				= SteamInput()->GetDigitalActionHandle( "menu_lb" );
	g_DA_MenuRB				= SteamInput()->GetDigitalActionHandle( "menu_rb" );
#endif

	SteamInput()->RunFrame();

	if (!m_bEnabled)
	{
		if (SteamUtils()->IsSteamRunningOnSteamDeck())
		{
			InputHandle_t inputHandles[STEAM_INPUT_MAX_COUNT];
			int iNumHandles = SteamInput()->GetConnectedControllers( inputHandles );
			Msg( "On Steam Deck and number of controllers is %i\n", iNumHandles );

			if (iNumHandles > 0)
			{
				DeckConnected( inputHandles[0] );
			}
		}
		else if (si_current_cfg.GetString()[0] != '0')
		{
			Msg("Reverting leftover Steam Input cvars\n");
			g_pEngineClient->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );
			g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_uninput_%s.cfg", si_current_cfg.GetString() ) );
		}
	}

	LoadHintRemap( "scripts/steaminput_hintremap.txt" );

	// Also load mod remap script
	LoadHintRemap( "scripts/mod_hintremap.txt" );

	g_pVGuiLocalize->AddFile( "resource/steaminput_%language%.txt" );
}

#define ACTION_MANIFEST_MOD					"steam_input/action_manifest_mod.vdf"
#define ACTION_MANIFEST_RELATIVE_HL2		"%s/../Half-Life 2/steam_input/action_manifest_hl2.vdf"
#define ACTION_MANIFEST_RELATIVE_PORTAL		"%s/../Portal/steam_input/action_manifest_hl2.vdf"

void CSource2013SteamInput::InitActionManifest()
{
	// First, check for a mod-specific action manifest
	if (g_pFullFileSystem->FileExists( ACTION_MANIFEST_MOD, "MOD" ))
	{
		char szFullPath[MAX_PATH];
		g_pFullFileSystem->RelativePathToFullPath( ACTION_MANIFEST_MOD, "MOD", szFullPath, sizeof( szFullPath ) );
		V_FixSlashes( szFullPath );

		Msg( "Loading mod action manifest file at \"%s\"\n", szFullPath );
		SteamInput()->SetInputActionManifestFilePath( szFullPath );
	}
#if USE_HL2_INSTALLATION
	else if (SteamUtils()->GetAppID() == 243730 || SteamUtils()->GetAppID() == 243750)
	{
		char szCurDir[MAX_PATH];
		g_pFullFileSystem->GetCurrentDirectory( szCurDir, sizeof( szCurDir ) );

		char szTargetApp[MAX_PATH];
		Q_snprintf( szTargetApp, sizeof( szTargetApp ), ACTION_MANIFEST_RELATIVE_HL2, szCurDir );
		V_FixSlashes( szTargetApp );

		if (g_pFullFileSystem->FileExists( szTargetApp ))
		{
			Msg( "Loading Half-Life 2 action manifest file at \"%s\"\n", szTargetApp );
			SteamInput()->SetInputActionManifestFilePath( szTargetApp );
		}
		else
		{
			// If Half-Life 2 is not installed, check if Portal has it
			Q_snprintf( szTargetApp, sizeof( szTargetApp ), ACTION_MANIFEST_RELATIVE_PORTAL, szCurDir );
			V_FixSlashes( szTargetApp );

			if (g_pFullFileSystem->FileExists( szTargetApp ))
			{
				Msg( "Loading Portal's copy of HL2 action manifest file at \"%s\"\n", szTargetApp );
				SteamInput()->SetInputActionManifestFilePath( szTargetApp );
			}
		}
	}
#endif
}

void CSource2013SteamInput::LoadActionBinds( const char *pszFileName )
{
	KeyValues *pKV = new KeyValues("ActionBinds");
	if ( pKV->LoadFromFile( g_pFullFileSystem, pszFileName ) )
	{
		// Parse each action bind
		KeyValues *pKVAction = pKV->GetFirstSubKey();
		while ( pKVAction )
		{
			InputDigitalActionHandle_t action = SteamInput()->GetDigitalActionHandle( pKVAction->GetName() );
			if ( action != 0 )
			{
				int i = g_DigitalActionBinds.AddToTail();
				g_DigitalActionBinds[i].handle = action;

				g_DigitalActionBindNames.CopyAndAddToTail( pKVAction->GetName() );
				g_DigitalActionBinds[i].pszActionName = g_DigitalActionBindNames.Tail();

				g_DigitalActionBindNames.CopyAndAddToTail( pKVAction->GetString() );
				g_DigitalActionBinds[i].pszBindCommand = g_DigitalActionBindNames.Tail();
			}
			else
			{
				Warning("Invalid Steam Input action \"%s\"\n", pKVAction->GetName());
			}

			pKVAction = pKVAction->GetNextKey();
		}
	}
	else
	{
		Msg( "SteamInput action bind file \"%s\" failed to load\n", pszFileName );
	}
	pKV->deleteThis();
}

InputDigitalActionCommandBind_t *CSource2013SteamInput::FindActionBind( const char *pszActionName )
{
	for (int i = 0; i < g_DigitalActionBinds.Count(); i++)
	{
		if (!V_strcmp( pszActionName, g_DigitalActionBinds[i].pszActionName ))
			return &g_DigitalActionBinds[i];
	}

	return NULL;
}

void CSource2013SteamInput::LevelInitPreEntity()
{
	if (IsEnabled())
	{
		// Sometimes, the archived value overwrites the cvar. This is a compromise to make sure that doesn't happen
		ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
		si_current_cfg.SetValue( IdentifyControllerParam( inputType ) );
	}
}

void CSource2013SteamInput::Shutdown()
{
	SteamInput()->Shutdown();
	m_nControllerHandle = 0;

	g_DigitalActionBindNames.PurgeAndDeleteElements();
	g_DigitalActionBinds.RemoveAll();
}

//-------------------------------------------

const char *CSource2013SteamInput::IdentifyControllerParam( ESteamInputType inputType )
{
	switch (inputType)
	{
		case k_ESteamInputType_SteamDeckController:
			return "deck";
			break;
		case k_ESteamInputType_SteamController:
			return "steamcontroller";
			break;
		case k_ESteamInputType_XBox360Controller:
			return "xbox360";
			break;
		case k_ESteamInputType_XBoxOneController:
			return "xboxone";
			break;
		case k_ESteamInputType_PS3Controller:
			return "ps3";
			break;
		case k_ESteamInputType_PS4Controller:
			return "ps4";
			break;
		case k_ESteamInputType_PS5Controller:
			return "ps5";
			break;
		case k_ESteamInputType_SwitchProController:
			return "switchpro";
			break;
		case k_ESteamInputType_SwitchJoyConPair:
			return "joyconpair";
			break;
		case k_ESteamInputType_SwitchJoyConSingle:
			return "joyconsingle";
			break;
	}

	return NULL;
}

void CSource2013SteamInput::InputDeviceConnected( InputHandle_t nDeviceHandle )
{
	m_nControllerHandle = nDeviceHandle;
	m_bEnabled = true;

	g_pEngineClient->ClientCmd_Unrestricted( "exec steam_input.cfg" );

	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
	const char *pszInputPrintType = IdentifyControllerParam( inputType );

	Msg( "Steam Input running with a controller (%i: %s)\n", inputType, pszInputPrintType );

	if (pszInputPrintType)
	{
		g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_input_%s.cfg", pszInputPrintType ) );
		si_current_cfg.SetValue( pszInputPrintType );
	}

	if (g_pEngineClient->IsConnected() )
	{
		// Refresh weapon buckets
		g_pEngineClient->ClientCmd_Unrestricted( "weapon_precache_weapon_info_database\n" );
	}
}

void CSource2013SteamInput::InputDeviceDisconnected( InputHandle_t nDeviceHandle )
{
	Msg( "Steam Input controller disconnected\n" );

	m_nControllerHandle = 0;
	m_bEnabled = false;

	g_pEngineClient->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );

	const char *pszInputPrintType = NULL;
	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( nDeviceHandle );
	pszInputPrintType = IdentifyControllerParam( inputType );

	if (pszInputPrintType)
	{
		g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_uninput_%s.cfg", pszInputPrintType ) );
	}

	si_current_cfg.SetValue( "0" );

	if ( g_pEngineClient->IsConnected() )
	{
		// Refresh weapon buckets
		g_pEngineClient->ClientCmd_Unrestricted( "weapon_precache_weapon_info_database\n" );
	}
}

void CSource2013SteamInput::InputDeviceChanged( InputHandle_t nOldHandle, InputHandle_t nNewHandle )
{
	// Disconnect previous controller
	const char *pszInputPrintType = NULL;
	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( nOldHandle );
	pszInputPrintType = IdentifyControllerParam( inputType );

	if (pszInputPrintType)
	{
		g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_uninput_%s.cfg", pszInputPrintType ) );
	}

	// Connect new controller
	m_nControllerHandle = nNewHandle;

	ESteamInputType newInputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
	const char *pszNewInputPrintType = IdentifyControllerParam( newInputType );

	Msg( "Steam Input changing controller from %i/%s to %i/%s\n", inputType, pszInputPrintType, newInputType, pszNewInputPrintType );

	if (pszNewInputPrintType)
	{
		g_pEngineClient->ClientCmd_Unrestricted( SteamInput_VarArgs( "exec steam_input_%s.cfg", pszNewInputPrintType ) );
		si_current_cfg.SetValue( pszNewInputPrintType );
	}
}

void CSource2013SteamInput::DeckConnected( InputHandle_t nDeviceHandle )
{
	Msg( "Steam Input running with a Steam Deck\n" );

	m_nControllerHandle = nDeviceHandle;
	m_bEnabled = true;

	g_pEngineClient->ClientCmd_Unrestricted( "exec steam_input.cfg" );
	g_pEngineClient->ClientCmd_Unrestricted( "exec steam_input_deck.cfg" );
	si_current_cfg.SetValue( "deck" );
}

//-------------------------------------------

void CSource2013SteamInput::RunFrame( ActionSet_t &iActionSet )
{
	if (g_DigitalActionBinds.Count() == 0)
		return;

	SteamInput()->RunFrame();

	static InputHandle_t inputHandles[STEAM_INPUT_MAX_COUNT];
	int iNumHandles = SteamInput()->GetConnectedControllers( inputHandles );

	//Msg( "Number of handles is %i!!! (inputHandles[0] is %llu, m_nControllerHandle is %llu)\n", iNumHandles, inputHandles[0], m_nControllerHandle );

	if (iNumHandles <= 0)
	{
		if (IsEnabled())
		{
			// No controllers available, disable Steam Input
			InputDeviceDisconnected( m_nControllerHandle );
		}
		return;
	}

	//if (!SteamInput()->BNewDataAvailable())
	//	return;

	// Reset the analog data
	m_analogMoveData = m_analogCameraData = InputAnalogActionData_t();

	InputHandle_t iFirstActive = m_nControllerHandle;
	for (int i = 0; i < iNumHandles; i++)
	{
		if (TestActions( iActionSet, inputHandles[i] ))
		{
			if (iFirstActive == m_nControllerHandle)
				iFirstActive = inputHandles[i];
		}
	}

	if (iFirstActive != m_nControllerHandle)
	{
		// Disconnect previous controller if its inputs are not active
		if (m_nControllerHandle != 0)
		{
			InputDeviceChanged( m_nControllerHandle, iFirstActive );
		}
		else
		{
			// Register the new controller
			InputDeviceConnected( iFirstActive );
		}
	}

	m_iLastActionSet = iActionSet;

	if (si_print_action_set.GetBool())
	{
		switch (iActionSet)
		{
			case AS_GameControls:
				Msg( "Steam Input: GameControls\n" );
				break;

			case AS_VehicleControls:
				Msg( "Steam Input: VehicleControls\n" );
				break;

			case AS_MenuControls:
				Msg( "Steam Input: MenuControls\n" );
				break;
		}
	}
}

bool CSource2013SteamInput::IsEnabled()
{
	return m_bEnabled;
}

bool CSource2013SteamInput::IsSteamRunningOnSteamDeck()
{
	return SteamUtils()->IsSteamRunningOnSteamDeck();
}

void CSource2013SteamInput::SetGamepadUI( bool bToggle )
{
	m_bIsGamepadUI = bToggle;
}

InputHandle_t CSource2013SteamInput::GetActiveController()
{
	return m_nControllerHandle;
}

int CSource2013SteamInput::GetConnectedControllers( InputHandle_t *nOutHandles )
{
	if (_ARRAYSIZE( nOutHandles ) < STEAM_INPUT_MAX_COUNT)
	{
		Warning( "ISource2013SteamInput::GetConnectedControllers requires an array greater than or equal to STEAM_INPUT_MAX_COUNT (%i) in size\n", STEAM_INPUT_MAX_COUNT );
		return 0;
	}

	return SteamInput()->GetConnectedControllers( nOutHandles );
}

const char *CSource2013SteamInput::GetControllerName( InputHandle_t nController )
{
	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( nController );
	return IdentifyControllerParam( inputType );
}

int CSource2013SteamInput::GetControllerType( InputHandle_t nController )
{
	return SteamInput()->GetInputTypeForHandle( nController );
}

bool CSource2013SteamInput::ShowBindingPanel( InputHandle_t nController )
{
	return SteamInput()->ShowBindingPanel( nController );
}

bool CSource2013SteamInput::TestActions( int iActionSet, InputHandle_t nController )
{
	bool bActiveInput = false;

	switch (iActionSet)
	{
		case AS_GameControls:
		{
			SteamInput()->ActivateActionSet( nController, g_AS_GameControls );
			
			// Run commands for all digital actions
			for (int i = 0; i < g_DigitalActionBinds.Count(); i++)
			{
				TestDigitalActionBind( nController, g_DigitalActionBinds[i], bActiveInput );
			}

			InputAnalogActionData_t moveData = SteamInput()->GetAnalogActionData( nController, g_AA_Move );
			if (!m_analogMoveData.bActive)
			{
				m_analogMoveData = moveData;
			}
			else if (m_analogMoveData.eMode == moveData.eMode)
			{
				// Just add on to existing input
				m_analogMoveData.x += moveData.x;
				m_analogMoveData.y += moveData.y;
			}

			if (moveData.x != 0.0f || moveData.y != 0.0f)
			{
				bActiveInput = true;
			}

		} break;

		case AS_VehicleControls:
		{
			SteamInput()->ActivateActionSet( nController, g_AS_VehicleControls );
			
			// Run commands for all digital actions
			for (int i = 0; i < g_DigitalActionBinds.Count(); i++)
			{
				TestDigitalActionBind( nController, g_DigitalActionBinds[i], bActiveInput );
			}

			InputAnalogActionData_t moveData = SteamInput()->GetAnalogActionData( nController, g_AA_Move );
			if (!m_analogMoveData.bActive)
			{
				m_analogMoveData = moveData;
			}
			else if (m_analogMoveData.eMode == moveData.eMode)
			{
				// Just add on to existing input
				m_analogMoveData.x += moveData.x;
				m_analogMoveData.y += moveData.y;
			}

			// Add steer data to the X value
			InputAnalogActionData_t steerData = SteamInput()->GetAnalogActionData( nController, g_AA_Steer );
			m_analogMoveData.x += steerData.x;

			// Add acceleration to the Y value
			steerData = SteamInput()->GetAnalogActionData( nController, g_AA_Accelerate );
			m_analogMoveData.y += steerData.x;

			if (g_DAB_Brake->bDown == false)
			{
				// For now, braking is equal to the digital action
				steerData = SteamInput()->GetAnalogActionData( nController, g_AA_Brake );
				if (steerData.x >= 0.25f)
				{
					g_pEngineClient->ClientCmd_Unrestricted( "+jump" );
				}
				else
				{
					g_pEngineClient->ClientCmd_Unrestricted( "-jump" );
				}
			}

			if (moveData.x != 0.0f || moveData.y != 0.0f ||
				steerData.x != 0.0f || steerData.y != 0.0f)
			{
				bActiveInput = true;
			}

		} break;

		case AS_MenuControls:
		{
			SteamInput()->ActivateActionSet( nController, g_AS_MenuControls );

			//if (!SteamInput()->BNewDataAvailable())
			//	break;

#if MENU_ACTIONS_ARE_BINDS
			if (m_bIsGamepadUI)
			{
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuUp, KEY_XBUTTON_UP, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuDown, KEY_XBUTTON_DOWN, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuLeft, KEY_XBUTTON_LEFT, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuRight, KEY_XBUTTON_RIGHT, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuSelect, KEY_XBUTTON_A, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuCancel, KEY_XBUTTON_B, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuX, KEY_XBUTTON_X, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuY, KEY_XBUTTON_Y, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuLB, KEY_XBUTTON_LEFT_SHOULDER, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuRB, KEY_XBUTTON_RIGHT_SHOULDER, bActiveInput );
			}
			else
			{
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuUp, KEY_UP, bActiveInput ); // KEY_XBUTTON_UP
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuDown, KEY_DOWN, bActiveInput ); // KEY_XBUTTON_DOWN
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuLeft, KEY_LEFT, bActiveInput ); // KEY_XBUTTON_LEFT
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuRight, KEY_RIGHT, bActiveInput ); // KEY_XBUTTON_RIGHT
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuSelect, KEY_XBUTTON_A, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuCancel, KEY_XBUTTON_B, bActiveInput );
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuLB, KEY_XBUTTON_LEFT, bActiveInput ); // KEY_XBUTTON_LEFT_SHOULDER
				PressKeyFromDigitalActionHandle( nController, g_DAB_MenuRB, KEY_XBUTTON_RIGHT, bActiveInput ); // KEY_XBUTTON_RIGHT_SHOULDER
			}
#else
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuUp, KEY_UP, bActiveInput ); // KEY_XBUTTON_UP
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuDown, KEY_DOWN, bActiveInput ); // KEY_XBUTTON_DOWN
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuLeft, KEY_LEFT, bActiveInput ); // KEY_XBUTTON_LEFT
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuRight, KEY_RIGHT, bActiveInput ); // KEY_XBUTTON_RIGHT
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuSelect, KEY_XBUTTON_A, bActiveInput );
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuCancel, KEY_XBUTTON_B, bActiveInput );
			//PressKeyFromDigitalActionHandle( nController, g_DA_MenuX, KEY_X, bActiveInput );
			//PressKeyFromDigitalActionHandle( nController, g_DA_MenuY, KEY_Y, bActiveInput );
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuLB, KEY_XBUTTON_LEFT, bActiveInput ); // KEY_XBUTTON_LEFT_SHOULDER
			PressKeyFromDigitalActionHandle( nController, g_DA_MenuRB, KEY_XBUTTON_RIGHT, bActiveInput ); // KEY_XBUTTON_RIGHT_SHOULDER
#endif

			TestDigitalActionBind( nController, *g_DAB_MenuPause, bActiveInput );

			if (!m_bIsGamepadUI)
			{
				//InputDigitalActionData_t xData = SteamInput()->GetDigitalActionData( m_nControllerHandle, g_DAB_MenuX.handle );
				InputDigitalActionData_t yData = SteamInput()->GetDigitalActionData( nController, g_DAB_MenuY.handle );

				//if (xData.bState)
				//	engine->ClientCmd_Unrestricted( "gamemenucommand OpenOptionsDialog\n" );

				if (yData.bState)
				{
					g_pEngineClient->ClientCmd_Unrestricted( "gamemenucommand OpenOptionsDialog\n" );
					bActiveInput = true;
				}
			}

		} break;
	}

	if (iActionSet != AS_MenuControls)
	{
		InputAnalogActionData_t cameraData = SteamInput()->GetAnalogActionData( nController, g_AA_Camera );
		InputAnalogActionData_t cameraJoystickData = SteamInput()->GetAnalogActionData( nController, g_AA_JoystickCamera );

		if (cameraJoystickData.bActive)
		{
			cameraData = cameraJoystickData;
		}

		if (!m_analogCameraData.bActive)
		{
			m_analogCameraData = cameraData;
		}
		else if (m_analogCameraData.eMode == cameraData.eMode)
		{
			// Just add on to existing input
			m_analogCameraData.x += cameraData.x;
			m_analogCameraData.y += cameraData.y;
		}

		if (cameraData.x != 0.0f || cameraData.y != 0.0f)
		{
			bActiveInput = true;
		}
	}

	//Msg( "Active input on %llu is %s\n", nController, bActiveInput ? "true" : "false" );

	return bActiveInput;
}

void CSource2013SteamInput::TestDigitalActionBind( InputHandle_t nController, InputDigitalActionBind_t &DigitalAction, bool &bActiveInput )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( nController, DigitalAction.handle );

	if (data.bState)
	{
		// Key is not down
		if (!DigitalAction.bDown)
		{
			DigitalAction.controller = nController;
			DigitalAction.bDown = true;
			DigitalAction.OnDown();
		}

		if (DigitalAction.controller == nController)
			bActiveInput = true;
	}
	else if (DigitalAction.controller == nController)
	{
		// Key was already down on this controller
		if (DigitalAction.bDown)
		{
			DigitalAction.bDown = false;
			DigitalAction.OnUp();
		}
	}
}

#if MENU_ACTIONS_ARE_BINDS
void CSource2013SteamInput::PressKeyFromDigitalActionHandle( InputHandle_t nController, InputDigitalActionBind_t &nHandle, ButtonCode_t nKey, bool &bActiveInput )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( nController, nHandle.handle );

	bool bSendKey = false;
	if (data.bState)
	{
		// Key is not down
		if (!nHandle.bDown)
		{
			nHandle.controller = nController;
			nHandle.bDown = true;
			bSendKey = true;
		}

		if (nHandle.controller == nController)
			bActiveInput = true;
	}
	else if (nHandle.controller == nController)
	{
		// Key is already down
		if (nHandle.bDown)
		{
			nHandle.bDown = false;
			bSendKey = true;
		}
	}
		
	if (bSendKey)
	{
		if (nHandle.bDown)
			vgui::ivgui()->PostMessage( vgui::input()->GetFocus(), new KeyValues( "KeyCodePressed", "code", nKey ), NULL );
		else
			vgui::ivgui()->PostMessage( vgui::input()->GetFocus(), new KeyValues( "KeyCodeReleased", "code", nKey ), NULL );
	}
}
#else
void CSource2013SteamInput::PressKeyFromDigitalActionHandle( InputDigitalActionHandle_t nHandle, ButtonCode_t nKey )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( m_nControllerHandle, nHandle );

	/*if (data.bActive)
	{
		//g_pClientMode->GetViewport()->OnKeyCodePressed( nKey );

		//InputEvent_t inputEvent;
		//inputEvent.m_nType = IE_ButtonPressed;
		//inputEvent.m_nData = nKey;
		//inputEvent.m_nData2 = inputsystem->ButtonCodeToVirtualKey( nKey );
		//inputsystem->PostUserEvent( inputEvent );
	}*/
}
#endif

static inline bool IsRelativeAnalog( EInputSourceMode mode )
{
	// TODO: Is there a better way of doing this?
	return mode == k_EInputSourceMode_AbsoluteMouse ||
		mode == k_EInputSourceMode_RelativeMouse ||
		mode == k_EInputSourceMode_JoystickMouse ||
		mode == k_EInputSourceMode_MouseRegion;
}

bool CSource2013SteamInput::UsingJoysticks()
{
	// For now, any controller uses joysticks
	return IsEnabled();
}

void CSource2013SteamInput::GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
	bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw )
{

	if (IsRelativeAnalog( m_analogMoveData.eMode ))
	{
		bRelativeForward = true;
		bRelativeSide = true;

		flForward = (m_analogMoveData.y / 180.0f) * MAX_BUTTONSAMPLE;
		flSide = (m_analogMoveData.x / 180.0f) * MAX_BUTTONSAMPLE;
	}
	else
	{
		bRelativeForward = false;
		bRelativeSide = false;

		flForward = m_analogMoveData.y * MAX_BUTTONSAMPLE;
		flSide = m_analogMoveData.x * MAX_BUTTONSAMPLE;
	}
	
	if (IsRelativeAnalog( m_analogCameraData.eMode ))
	{
		bRelativePitch = true;
		bRelativeYaw = true;

		flPitch = (m_analogCameraData.y / 180.0f) * MAX_BUTTONSAMPLE;
		flYaw = (m_analogCameraData.x / 180.0f) * MAX_BUTTONSAMPLE;
	}
	else
	{
		bRelativePitch = false;
		bRelativeYaw = false;

		flPitch = m_analogCameraData.y * MAX_BUTTONSAMPLE;
		flYaw = m_analogCameraData.x * MAX_BUTTONSAMPLE;
	}

	if (si_print_joy_src.GetBool())
	{
		Msg( "moveData = %i (%f, %f)\ncameraData = %i (%f, %f)\n\n",
			m_analogMoveData.eMode, m_analogMoveData.x, m_analogMoveData.y,
			m_analogCameraData.eMode, m_analogCameraData.x, m_analogCameraData.y );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void CSource2013SteamInput::SetRumble( InputHandle_t nController, float fLeftMotor, float fRightMotor, int userId )
{
	if (!IsEnabled() || !si_enable_rumble.GetBool())
	{
		g_pInputSystem->SetRumble( fLeftMotor, fRightMotor, userId );
		return;
	}

	if (nController == 0)
		nController = m_nControllerHandle;

	SteamInput()->TriggerVibrationExtended( nController, fLeftMotor, fRightMotor, fLeftMotor, fRightMotor );

	if (si_print_rumble.GetBool())
	{
		Msg( "fLeftMotor = %f, fRightMotor = %f\n\n", fLeftMotor, fRightMotor );
	}
}

void CSource2013SteamInput::StopRumble()
{
	if (!IsEnabled())
	{
		g_pInputSystem->StopRumble();
		return;
	}

	// N/A
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void CSource2013SteamInput::SetLEDColor( InputHandle_t nController, byte r, byte g, byte b )
{
	SteamInput()->SetLEDColor( nController, r, g, b, k_ESteamInputLEDFlag_SetColor );
}

void CSource2013SteamInput::ResetLEDColor( InputHandle_t nController )
{
	SteamInput()->SetLEDColor( nController, 0, 0, 0, k_ESteamInputLEDFlag_RestoreUserDefault );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int CSource2013SteamInput::FindDigitalActionsForCommand( const char *pszCommand, InputDigitalActionHandle_t *pHandles )
{
	int iNumHandles = 0;

	if (m_iLastActionSet == AS_MenuControls)
	{
		pszCommand += 5;
		if (!V_strcmp( pszCommand, "up" ))		{ pHandles[0] = g_DAB_MenuUp.handle; return 1; }
		if (!V_strcmp( pszCommand, "down" ))	{ pHandles[0] = g_DAB_MenuDown.handle; return 1; }
		if (!V_strcmp( pszCommand, "left" ))	{ pHandles[0] = g_DAB_MenuLeft.handle; return 1; }
		if (!V_strcmp( pszCommand, "right" ))	{ pHandles[0] = g_DAB_MenuRight.handle; return 1; }
		if (!V_strcmp( pszCommand, "select" ))	{ pHandles[0] = g_DAB_MenuSelect.handle; return 1; }
		if (!V_strcmp( pszCommand, "cancel" ))	{ pHandles[0] = g_DAB_MenuCancel.handle; return 1; }
		if (!V_strcmp( pszCommand, "x" ))		{ pHandles[0] = g_DAB_MenuX.handle; return 1; }
		if (!V_strcmp( pszCommand, "y" ))		{ pHandles[0] = g_DAB_MenuY.handle; return 1; }
		if (!V_strcmp( pszCommand, "lb" ))		{ pHandles[0] = g_DAB_MenuLB.handle; return 1; }
		if (!V_strcmp( pszCommand, "rb" ))		{ pHandles[0] = g_DAB_MenuRB.handle; return 1; }
	}

	// Special cases
	if (!V_strcmp( pszCommand, "duck" ))
	{
		// Add toggle_duck
		pHandles[iNumHandles] = g_DAB_Toggle_Duck->handle;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "zoom" ))
	{
		// Add toggle_zoom
		pHandles[iNumHandles] = g_DAB_Toggle_Zoom->handle;
		iNumHandles++;
	}

	// Figure out which command this is
	for (int i = 0; i < g_DigitalActionBinds.Count(); i++)
	{
		const char *pszBindCommand = g_DigitalActionBinds[i].pszBindCommand;
		if (pszBindCommand[0] == '+')
			pszBindCommand++;

		if (!V_strcmp( pszCommand, pszBindCommand ))
		{
			pHandles[iNumHandles] = g_DigitalActionBinds[i].handle;
			iNumHandles++;
			break;
		}
	}

	return iNumHandles;
}

int CSource2013SteamInput::FindAnalogActionsForCommand( const char *pszCommand, InputAnalogActionHandle_t *pHandles )
{
	int iNumHandles = 0;

	// Check pre-set analog action names
	if (!V_strcmp( pszCommand, "xlook" ))
	{
		// Add g_AA_Camera and g_AA_JoystickCamera
		pHandles[iNumHandles] = g_AA_Camera;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_JoystickCamera;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "xaccel" ))
	{
		// Add g_AA_Accelerate and g_AA_Move
		pHandles[iNumHandles] = g_AA_Accelerate;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_Move;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "xmove" ) )
	{
		// Add g_AA_Accelerate and g_AA_Move
		pHandles[iNumHandles] = g_AA_Accelerate;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_Move;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "xsteer" ))
	{
		pHandles[iNumHandles] = g_AA_Steer;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "xbrake" ))
	{
		pHandles[iNumHandles] = g_AA_Brake;
		iNumHandles++;
	}
	else if (!V_strcmp( pszCommand, "xmouse" ))
	{
		pHandles[iNumHandles] = g_AA_Mouse;
		iNumHandles++;
	}

	return iNumHandles;
}

void CSource2013SteamInput::GetInputActionOriginsForCommand( const char *pszCommand, CUtlVector<EInputActionOrigin> &actionOrigins, int iActionSetOverride )
{
	InputActionSetHandle_t actionSet = g_AS_MenuControls;

	if (iActionSetOverride != -1)
	{
		switch (iActionSetOverride)
		{
			default:
			case AS_GameControls:		actionSet = g_AS_GameControls; break;
			case AS_VehicleControls:	actionSet = g_AS_VehicleControls; break;
			//case AS_MenuControls:		actionSet = g_AS_MenuControls; break;
		}
	}
	else
	{
		switch (m_iLastActionSet)
		{
			case AS_GameControls:		actionSet = g_AS_GameControls; break;
			case AS_VehicleControls:	actionSet = g_AS_VehicleControls; break;
		}
	}

	InputDigitalActionHandle_t digitalActions[STEAM_INPUT_MAX_ORIGINS];
	int iNumActions = FindDigitalActionsForCommand( pszCommand, digitalActions );
	if (iNumActions > 0)
	{
		for (int i = 0; i < iNumActions; i++)
		{
			EInputActionOrigin actionOriginsLocal[STEAM_INPUT_MAX_ORIGINS];
			int iNumOriginsLocal = SteamInput()->GetDigitalActionOrigins( m_nControllerHandle, actionSet, digitalActions[i], actionOriginsLocal );

			if (iNumOriginsLocal > 0)
			{
				// Add them to the list
				actionOrigins.AddMultipleToTail( iNumOriginsLocal, actionOriginsLocal );

				//memcpy( actionOrigins+iNumOrigins, actionOriginsLocal, sizeof(EInputActionOrigin)*iNumOriginsLocal );
				//iNumOrigins += iNumOriginsLocal;
			}
		}
	}
	else
	{
		InputAnalogActionHandle_t analogActions[STEAM_INPUT_MAX_ORIGINS];
		iNumActions = FindAnalogActionsForCommand( pszCommand, analogActions );
		for (int i = 0; i < iNumActions; i++)
		{
			EInputActionOrigin actionOriginsLocal[STEAM_INPUT_MAX_ORIGINS];
			int iNumOriginsLocal = SteamInput()->GetAnalogActionOrigins( m_nControllerHandle, actionSet, analogActions[i], actionOriginsLocal );

			if (iNumOriginsLocal > 0)
			{
				// Add them to the list
				actionOrigins.AddMultipleToTail( iNumOriginsLocal, actionOriginsLocal );

				//memcpy( actionOrigins+iNumOrigins, actionOriginsLocal, sizeof(EInputActionOrigin)*iNumOriginsLocal );
				//iNumOrigins += iNumOriginsLocal;
			}
		}
	}
}

void CSource2013SteamInput::GetGlyphPNGsForCommand( CUtlVector<const char *> &szImgList, const char *pszCommand, int &iSize, int iStyle )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins );

	ESteamInputGlyphSize glyphSize;
	if (iSize <= 32)
	{
		glyphSize = k_ESteamInputGlyphSize_Small;
		iSize = 32;
	}
	else if (iSize <= 128)
	{
		glyphSize = k_ESteamInputGlyphSize_Medium;
		iSize = 128;
	}
	else
	{
		glyphSize = k_ESteamInputGlyphSize_Large;
		iSize = 256;
	}

	FOR_EACH_VEC( actionOrigins, i )
	{
		if (si_force_glyph_controller.GetInt() != -1)
		{
			actionOrigins[i] = SteamInput()->TranslateActionOrigin( (ESteamInputType)si_force_glyph_controller.GetInt(), actionOrigins[i] );
		}

		szImgList.AddToTail( SteamInput()->GetGlyphPNGForActionOrigin( actionOrigins[i], glyphSize, (ESteamInputGlyphStyle)iStyle ) );
	}
}

void CSource2013SteamInput::GetGlyphSVGsForCommand( CUtlVector<const char *> &szImgList, const char *pszCommand )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins );

	FOR_EACH_VEC( actionOrigins, i )
	{
		if (si_force_glyph_controller.GetInt() != -1)
		{
			actionOrigins[i] = SteamInput()->TranslateActionOrigin( (ESteamInputType)si_force_glyph_controller.GetInt(), actionOrigins[i] );
		}

		szImgList.AddToTail( SteamInput()->GetGlyphSVGForActionOrigin( actionOrigins[i], 0 ) );
	}
}

void CSource2013SteamInput::GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char*> &szStringList, int iActionSet )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins, iActionSet );

	for (int i = 0; i < actionOrigins.Count(); i++)
	{
		szStringList.AddToTail( LookupDescriptionForActionOrigin( actionOrigins[i] ) );
	}
}

inline const char *CSource2013SteamInput::LookupDescriptionForActionOrigin( EInputActionOrigin eAction )
{
	return SteamInput()->GetStringForActionOrigin( eAction );
}

//-----------------------------------------------------------------------------

#ifdef PNG_LIBPNG_VER
void ReadPNG_CUtlBuffer( png_structp png_ptr, png_bytep data, size_t length )
{
	if ( !png_ptr )
		return;

	CUtlBuffer *pBuffer = (CUtlBuffer *)png_get_io_ptr( png_ptr );

	if ( (size_t)pBuffer->TellMaxPut() < ( (size_t)pBuffer->TellGet() + length ) ) // CUtlBuffer::CheckGet()
	{
		//png_error( png_ptr, "read error" );
		png_longjmp( png_ptr, 1 );
	}

	pBuffer->Get( data, length );
}
#endif

bool CSource2013SteamInput::GetPNGBufferFromFile( const char *filename, CUtlMemory< byte > &buffer )
{
#ifdef PNG_LIBPNG_VER
	// Read the whole image to memory
	CUtlBuffer fileBuffer;

	if ( !g_pFullFileSystem->ReadFile( filename, NULL, fileBuffer ) )
	{
		Warning( "Failed to read PNG file (%s)\n", filename );
		return false;
	}

	if ( png_sig_cmp( (png_const_bytep)fileBuffer.Base(), 0, 8 ) )
	{
		Warning( "Bad PNG signature\n" );
		return false;
	}

	png_bytepp row_pointers = NULL;

	png_structp png_ptr = png_create_read_struct( png_get_libpng_ver(NULL), NULL, NULL, NULL );
	png_infop info_ptr = png_create_info_struct( png_ptr );

	if ( !info_ptr || !png_ptr )
	{
		Warning( "Out of memory reading PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		Warning( "Failed to read PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		if ( row_pointers )
			free( row_pointers );
		return false;
	}

	png_set_read_fn( png_ptr, &fileBuffer, ReadPNG_CUtlBuffer );
	png_read_info( png_ptr, info_ptr );

	png_uint_32 image_width, image_height;
	int bit_depth, color_type;

	png_get_IHDR( png_ptr, info_ptr, &image_width, &image_height, &bit_depth, &color_type, NULL, NULL, NULL );

	// expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
	// transparency chunks to full alpha channel; strip 16-bit-per-sample
	// images to 8 bits per sample; and convert grayscale to RGB[A]

	if ( color_type == PNG_COLOR_TYPE_PALETTE )
		png_set_expand(png_ptr);
	if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
		png_set_expand(png_ptr);
	if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
		png_set_expand(png_ptr);
#ifdef PNG_READ_16_TO_8_SUPPORTED
	if ( bit_depth == 16 )
	#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png_ptr);
	#else
		png_set_strip_16(png_ptr);
	#endif
#endif
	if ( color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb(png_ptr);

	// Expand RGB to RGBA
	if ( color_type == PNG_COLOR_TYPE_RGB )
		png_set_filler( png_ptr, 0xffff, PNG_FILLER_AFTER );

	png_read_update_info( png_ptr, info_ptr );

	png_uint_32 rowbytes = png_get_rowbytes( png_ptr, info_ptr );
	int channels = (int)png_get_channels( png_ptr, info_ptr );

	if ( channels != 4 )
	{
		Warning( "PNG is not RGBA\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( image_height > ((size_t)(-1)) / rowbytes )
	{
		Warning( "PNG data buffer would be too large\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	buffer.Init( 0, rowbytes * image_height );
	row_pointers = (png_bytepp)malloc( image_height * sizeof(png_bytep) );

	Assert( buffer.Base() && row_pointers );

	if ( !row_pointers )
	{
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	for ( png_uint_32 i = 0; i < image_height; ++i )
		row_pointers[i] = buffer.Base() + i*rowbytes;

	png_read_image( png_ptr, row_pointers );
	//png_read_end( png_ptr, NULL );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	free( row_pointers );

	return true;
#endif
}

//-----------------------------------------------------------------------------

void CSource2013SteamInput::LoadHintRemap( const char *pszFileName )
{
	KeyValues *pKV = new KeyValues("HintRemap");
	if ( pKV->LoadFromFile( g_pFullFileSystem, pszFileName ) )
	{
		// Parse each hint to remap
		KeyValues *pKVHint = pKV->GetFirstSubKey();
		while ( pKVHint )
		{
			// Parse hint remaps
			KeyValues *pKVRemappedHint = pKVHint->GetFirstSubKey();
			while ( pKVRemappedHint )
			{
				int i = m_HintRemaps.AddToTail();
			
				m_HintRemaps[i].pszOldHint = pKVHint->GetName();
				if (m_HintRemaps[i].pszOldHint[0] == '#')
					m_HintRemaps[i].pszOldHint++;

				m_HintRemaps[i].pszNewHint = pKVRemappedHint->GetName();

				// Parse remap conditions
				KeyValues *pKVRemapCond = pKVRemappedHint->GetFirstSubKey();
				while ( pKVRemapCond )
				{
					int i2 = m_HintRemaps[i].nRemapConds.AddToTail();

					const char *pszParam = pKVRemapCond->GetString();
					if (pszParam[0] == '!')
					{
						m_HintRemaps[i].nRemapConds[i2].bNot = true;
						pszParam++;
					}

					Q_strncpy( m_HintRemaps[i].nRemapConds[i2].szParam, pszParam, sizeof( m_HintRemaps[i].nRemapConds[i2].szParam ) );

					if (!V_strcmp( pKVRemapCond->GetName(), "if_input_type" ))
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_INPUT_TYPE;
					else if (!V_strcmp( pKVRemapCond->GetName(), "if_action_bound" ))
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_ACTION_BOUND;
					else
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_NONE;

					pKVRemapCond = pKVRemapCond->GetNextKey();
				}

				pKVRemappedHint = pKVRemappedHint->GetNextKey();
			}

			pKVHint = pKVHint->GetNextKey();
		}
	}
	pKV->deleteThis();
}

void CSource2013SteamInput::RemapHudHint( const char **pszInputHint )
{
	if (!si_hintremap.GetBool())
		return;

	if ((*pszInputHint)[0] == '#')
		(*pszInputHint)++;

	int iRemap = -1;

	for (int i = 0; i < m_HintRemaps.Count(); i++)
	{
		if (V_strcmp( *pszInputHint, m_HintRemaps[i].pszOldHint ))
			continue;

		// If we've already selected a remap, ignore ones without conditions
		if (iRemap != -1 && m_HintRemaps[i].nRemapConds.Count() <= 0)
			continue;

		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Testing hint remap for %s to %s...\n", *pszInputHint, m_HintRemaps[i].pszNewHint );

		bool bPass = true;

		for (int i2 = 0; i2 < m_HintRemaps[i].nRemapConds.Count(); i2++)
		{
			if (si_print_hintremap.GetBool())
				Msg( "	Hint Remap: Testing remap condition %i (param %s)\n", m_HintRemaps[i].nRemapConds[i2].iType, m_HintRemaps[i].nRemapConds[i2].szParam );

			switch (m_HintRemaps[i].nRemapConds[i2].iType)
			{
				case HINTREMAPCOND_INPUT_TYPE:
				{
					ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
					bPass = !V_strcmp( IdentifyControllerParam( inputType ), m_HintRemaps[i].nRemapConds[i2].szParam );
				} break;

				case HINTREMAPCOND_ACTION_BOUND:
				{
					CUtlVector<EInputActionOrigin> actionOrigins;
					GetInputActionOriginsForCommand( m_HintRemaps[i].nRemapConds[i2].szParam, actionOrigins );
					bPass = (actionOrigins.Count() > 0);
				} break;
			}

			if (m_HintRemaps[i].nRemapConds[i2].bNot)
				bPass = !bPass;

			if (!bPass)
				break;
		}

		if (bPass)
		{
			if (si_print_hintremap.GetBool())
				Msg( "Hint Remap: Hint remap for %s to %s succeeded\n", *pszInputHint, m_HintRemaps[i].pszNewHint );

			iRemap = i;
		}
		else if (si_print_hintremap.GetBool())
		{
			Msg( "Hint Remap: Hint remap for %s to %s did not pass\n", *pszInputHint, m_HintRemaps[i].pszNewHint );
		}
	}

	if (iRemap != -1)
	{
		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Remapping hint %s to %s\n", *pszInputHint, m_HintRemaps[iRemap].pszNewHint );

		*pszInputHint = m_HintRemaps[iRemap].pszNewHint;
	}
	else
	{
		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Didn't find a hint for %s to remap to\n", *pszInputHint );
	}
}

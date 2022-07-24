//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base combat character with no AI
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASECOMBATCHARACTER_H
#define BASECOMBATCHARACTER_H

#include <limits.h>
#include "weapon_proficiency.h"

#ifdef _WIN32
#pragma once
#endif

#ifdef INVASION_DLL
#include "tf_shareddefs.h"

#define POWERUP_THINK_CONTEXT	"PowerupThink"
#endif

#include "cbase.h"
#include "baseentity.h"
#include "baseflex.h"
#include "damagemodifier.h"
#include "utllinkedlist.h"
#include "ai_hull.h"
#include "ai_utils.h"
#include "physics_impact_damage.h"

class CNavArea;
class CScriptedTarget;
typedef CHandle<CBaseCombatWeapon> CBaseCombatWeaponHandle;

// -------------------------------------
//  Capability Bits
// -------------------------------------

enum Capability_t 
{
	bits_CAP_MOVE_GROUND			= 0x00000001, // walk/run
	bits_CAP_MOVE_JUMP				= 0x00000002, // jump/leap
	bits_CAP_MOVE_FLY				= 0x00000004, // can fly, move all around
	bits_CAP_MOVE_CLIMB				= 0x00000008, // climb ladders
	bits_CAP_MOVE_SWIM				= 0x00000010, // navigate in water			// UNDONE - not yet implemented
	bits_CAP_MOVE_CRAWL				= 0x00000020, // crawl						// UNDONE - not yet implemented
	bits_CAP_MOVE_SHOOT				= 0x00000040, // tries to shoot weapon while moving
	bits_CAP_SKIP_NAV_GROUND_CHECK	= 0x00000080, // optimization - skips ground tests while computing navigation
	bits_CAP_USE					= 0x00000100, // open doors/push buttons/pull levers
	//bits_CAP_HEAR					= 0x00000200, // can hear forced sounds
	bits_CAP_AUTO_DOORS				= 0x00000400, // can trigger auto doors
	bits_CAP_OPEN_DOORS				= 0x00000800, // can open manual doors
	bits_CAP_TURN_HEAD				= 0x00001000, // can turn head, always bone controller 0
	bits_CAP_WEAPON_RANGE_ATTACK1	= 0x00002000, // can do a weapon range attack 1
	bits_CAP_WEAPON_RANGE_ATTACK2	= 0x00004000, // can do a weapon range attack 2
	bits_CAP_WEAPON_MELEE_ATTACK1	= 0x00008000, // can do a weapon melee attack 1
	bits_CAP_WEAPON_MELEE_ATTACK2	= 0x00010000, // can do a weapon melee attack 2
	bits_CAP_INNATE_RANGE_ATTACK1	= 0x00020000, // can do a innate range attack 1
	bits_CAP_INNATE_RANGE_ATTACK2	= 0x00040000, // can do a innate range attack 1
	bits_CAP_INNATE_MELEE_ATTACK1	= 0x00080000, // can do a innate melee attack 1
	bits_CAP_INNATE_MELEE_ATTACK2	= 0x00100000, // can do a innate melee attack 1
	bits_CAP_USE_WEAPONS			= 0x00200000, // can use weapons (non-innate attacks)
	//bits_CAP_STRAFE					= 0x00400000, // strafe ( walk/run sideways)
	bits_CAP_ANIMATEDFACE			= 0x00800000, // has animated eyes/face
	bits_CAP_USE_SHOT_REGULATOR		= 0x01000000, // Uses the shot regulator for range attack1
	bits_CAP_FRIENDLY_DMG_IMMUNE	= 0x02000000, // don't take damage from npc's that are D_LI
	bits_CAP_SQUAD					= 0x04000000, // can form squads
	bits_CAP_DUCK					= 0x08000000, // cover and reload ducking
	bits_CAP_NO_HIT_PLAYER			= 0x10000000, // don't hit players
	bits_CAP_AIM_GUN				= 0x20000000, // Use arms to aim gun, not just body
	bits_CAP_NO_HIT_SQUADMATES		= 0x40000000, // none
	bits_CAP_SIMPLE_RADIUS_DAMAGE	= 0x80000000, // Do not use robust radius damage model on this character.
};

#define bits_CAP_DOORS_GROUP    (bits_CAP_AUTO_DOORS | bits_CAP_OPEN_DOORS)
#define bits_CAP_RANGE_ATTACK_GROUP	(bits_CAP_WEAPON_RANGE_ATTACK1 | bits_CAP_WEAPON_RANGE_ATTACK2)
#define bits_CAP_MELEE_ATTACK_GROUP	(bits_CAP_WEAPON_MELEE_ATTACK1 | bits_CAP_WEAPON_MELEE_ATTACK2)


class CBaseCombatWeapon;

#define BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE 0.9f

enum Disposition_t 
{
	D_ER,		// Undefined - error
	D_HT,		// Hate
	D_FR,		// Fear
	D_LI,		// Like
	D_NU		// Neutral
};

const int DEF_RELATIONSHIP_PRIORITY = INT_MIN;

struct Relationship_t
{
	EHANDLE			entity;			// Relationship to a particular entity
	Class_T			classType;		// Relationship to a class  CLASS_NONE = not class based (Def. in baseentity.h)
	Disposition_t	disposition;	// D_HT (Hate), D_FR (Fear), D_LI (Like), D_NT (Neutral)
	int				priority;		// Relative importance of this relationship (higher numbers mean more important)

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: This should contain all of the combat entry points / functionality 
// that are common between NPCs and players
//-----------------------------------------------------------------------------
class CBaseCombatCharacter : public CBaseFlex
{
	DECLARE_CLASS( CBaseCombatCharacter, CBaseFlex );

public:
	CBaseCombatCharacter(void);
	~CBaseCombatCharacter(void);

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();
#ifdef MAPBASE_VSCRIPT
	DECLARE_ENT_SCRIPTDESC();
#endif

public:

	virtual void		Spawn( void );
	virtual void		Precache();

	virtual int			Restore( IRestore &restore );

	virtual const impactdamagetable_t	&GetPhysicsImpactDamageTable( void );

	int					TakeHealth( float flHealth, int bitsDamageType );
	void				CauseDeath( const CTakeDamageInfo &info );

	virtual	bool		FVisible ( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL ); // true iff the parameter can be seen by me.
	virtual bool		FVisible( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL )	{ return BaseClass::FVisible( vecTarget, traceMask, ppBlocker ); }
	static void			ResetVisibilityCache( CBaseCombatCharacter *pBCC = NULL );

#ifdef MAPBASE
	virtual bool		ShouldUseVisibilityCache( CBaseEntity *pEntity );
#endif

#ifdef PORTAL
	virtual	bool		FVisibleThroughPortal( const CProp_Portal *pPortal, CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );
#endif

	virtual bool		FInViewCone( CBaseEntity *pEntity );
	virtual bool		FInViewCone( const Vector &vecSpot );

#ifdef PORTAL
	virtual CProp_Portal*	FInViewConeThroughPortal( CBaseEntity *pEntity );
	virtual CProp_Portal*	FInViewConeThroughPortal( const Vector &vecSpot );
#endif

	virtual bool		FInAimCone( CBaseEntity *pEntity );
	virtual bool		FInAimCone( const Vector &vecSpot );
	
	virtual bool		ShouldShootMissTarget( CBaseCombatCharacter *pAttacker );
	virtual CBaseEntity *FindMissTarget( void );

#ifndef MAPBASE // This function now exists in CBaseEntity
	// Do not call HandleInteraction directly, use DispatchInteraction
	bool				DispatchInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )	{ return ( interactionType > 0 ) ? HandleInteraction( interactionType, data, sourceEnt ) : false; }
#endif
	virtual bool		HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

	virtual QAngle		BodyAngles();
	virtual Vector		BodyDirection2D( void );
	virtual Vector		BodyDirection3D( void );
	virtual Vector		HeadDirection2D( void )	{ return BodyDirection2D( ); }; // No head motion so just return body dir
	virtual Vector		HeadDirection3D( void )	{ return BodyDirection2D( ); }; // No head motion so just return body dir
	virtual Vector		EyeDirection2D( void ) 	{ return HeadDirection2D( );  }; // No eye motion so just return head dir
	virtual Vector		EyeDirection3D( void ) 	{ return HeadDirection3D( );  }; // No eye motion so just return head dir

	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	// -----------------------
	// Fog
	// -----------------------
	virtual bool		IsHiddenByFog( const Vector &target ) const;	///< return true if given target cant be seen because of fog
	virtual bool		IsHiddenByFog( CBaseEntity *target ) const;		///< return true if given target cant be seen because of fog
	virtual bool		IsHiddenByFog( float range ) const;				///< return true if given distance is too far to see through the fog
	virtual float		GetFogObscuredRatio( const Vector &target ) const;///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured
	virtual float		GetFogObscuredRatio( CBaseEntity *target ) const;	///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured
	virtual float		GetFogObscuredRatio( float range ) const;		///< return 0-1 ratio where zero is not obscured, and 1 is completely obscured


	// -----------------------
	// Vision
	// -----------------------
	enum FieldOfViewCheckType { USE_FOV, DISREGARD_FOV };

	// Visible starts with line of sight, and adds all the extra game checks like fog, smoke, camo...
	bool IsAbleToSee( const CBaseEntity *entity, FieldOfViewCheckType checkFOV );
	bool IsAbleToSee( CBaseCombatCharacter *pBCC, FieldOfViewCheckType checkFOV );

	virtual bool IsLookingTowards( const CBaseEntity *target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.
	virtual bool IsLookingTowards( const Vector &target, float cosTolerance = BCC_DEFAULT_LOOK_TOWARDS_TOLERANCE ) const;	// return true if our view direction is pointing at the given target, within the cosine of the angular tolerance. LINE OF SIGHT IS NOT CHECKED.

	virtual bool IsInFieldOfView( CBaseEntity *entity ) const;	// Calls IsLookingTowards with the current field of view.  
	virtual bool IsInFieldOfView( const Vector &pos ) const;

	enum LineOfSightCheckType
	{
		IGNORE_NOTHING,
		IGNORE_ACTORS
	};
	virtual bool IsLineOfSightClear( CBaseEntity *entity, LineOfSightCheckType checkType = IGNORE_NOTHING ) const;// strictly LOS check with no other considerations
	virtual bool IsLineOfSightClear( const Vector &pos, LineOfSightCheckType checkType = IGNORE_NOTHING, CBaseEntity *entityToIgnore = NULL ) const;

	// -----------------------
	// Ammo
	// -----------------------
	virtual int			GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound = false );
	int					GiveAmmo( int iCount, const char *szName, bool bSuppressSound = false );
	virtual void		RemoveAmmo( int iCount, int iAmmoIndex );
	virtual void		RemoveAmmo( int iCount, const char *szName );
	void				RemoveAllAmmo( );
	virtual int			GetAmmoCount( int iAmmoIndex ) const;
	int					GetAmmoCount( char *szName ) const;

	virtual Activity	NPC_TranslateActivity( Activity baseAct );

	// -----------------------
	// Weapons
	// -----------------------
	CBaseCombatWeapon*	Weapon_Create( const char *pWeaponName );
	virtual Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired = NULL );
	void				Weapon_SetActivity( Activity newActivity, float duration );
	virtual void		Weapon_FrameUpdate( void );
	virtual void		Weapon_HandleAnimEvent( animevent_t *pEvent );
	CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual bool		Weapon_CanUse( CBaseCombatWeapon *pWeapon );		// True is allowed to use this class of weapon
#ifdef MAPBASE
	virtual Activity	Weapon_BackupActivity( Activity activity, bool weaponTranslationWasRequired = false, CBaseCombatWeapon *pSpecificWeapon = NULL );
	virtual void		Weapon_Equip( CBaseCombatWeapon *pWeapon );			// Adds weapon to player
	virtual void		Weapon_EquipHolstered( CBaseCombatWeapon *pWeapon );	// Pretty much only useful for NPCs
	virtual void		Weapon_HandleEquip( CBaseCombatWeapon *pWeapon );
#else
	virtual void		Weapon_Equip( CBaseCombatWeapon *pWeapon );			// Adds weapon to player
#endif
	virtual bool		Weapon_EquipAmmoOnly( CBaseCombatWeapon *pWeapon );	// Adds weapon ammo to player, leaves weapon
	bool				Weapon_Detach( CBaseCombatWeapon *pWeapon );		// Clear any pointers to the weapon.
	virtual void		Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	virtual	bool		Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	virtual	Vector		Weapon_ShootPosition( );		// gun position at current position/orientation
	bool				Weapon_IsOnGround( CBaseCombatWeapon *pWeapon );
	CBaseEntity*		Weapon_FindUsable( const Vector &range );			// search for a usable weapon in this range
	virtual	bool		Weapon_CanSwitchTo(CBaseCombatWeapon *pWeapon);
	virtual bool		Weapon_SlotOccupied( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon *Weapon_GetSlot( int slot ) const;
	CBaseCombatWeapon	*Weapon_GetWpnForAmmo( int iAmmoIndex );


	// For weapon strip
	void				Weapon_DropAll( bool bDisallowWeaponPickup = false );

	virtual bool			AddPlayerItem( CBaseCombatWeapon *pItem ) { return false; }
	virtual bool			RemovePlayerItem( CBaseCombatWeapon *pItem ) { return false; }

	virtual bool			CanBecomeServerRagdoll( void ) { return true; }

	// -----------------------
	// Damage
	// -----------------------
	// Don't override this for characters, override the per-life-state versions below
	virtual int				OnTakeDamage( const CTakeDamageInfo &info );

	// Override these to control how your character takes damage in different states
	virtual int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual int				OnTakeDamage_Dying( const CTakeDamageInfo &info );
	virtual int				OnTakeDamage_Dead( const CTakeDamageInfo &info );

	virtual float			GetAliveDuration( void ) const;			// return time we have been alive (only valid when alive)

	virtual void 			OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttacker ) {}
	virtual void 			NotifyFriendsOfDamage( CBaseEntity *pAttackerEntity ) {}
	virtual bool			HasEverBeenInjured( int team = TEAM_ANY ) const;			// return true if we have ever been injured by a member of the given team
	virtual float			GetTimeSinceLastInjury( int team = TEAM_ANY ) const;		// return time since we were hurt by a member of the given team


	virtual void			OnPlayerKilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info ) {}

		// utility function to calc damage force
	Vector					CalcDamageForceVector( const CTakeDamageInfo &info );

	virtual int				BloodColor();
	virtual Activity		GetDeathActivity( void );

	virtual bool			CorpseGib( const CTakeDamageInfo &info );
	virtual void			CorpseFade( void );	// Called instead of GibNPC() when gibs are disabled
	virtual bool			HasHumanGibs( void );
	virtual bool			HasAlienGibs( void );
	virtual bool			ShouldGib( const CTakeDamageInfo &info ) { return false; }	// Always ragdoll, unless specified by the leaf class

	float GetDamageAccumulator() { return m_flDamageAccumulator; }
	int	  GetDamageCount( void ) { return m_iDamageCount; }	// # of times NPC has been damaged.  used for tracking 1-shot kills.

	// Character killed (only fired once)
	virtual void			Event_Killed( const CTakeDamageInfo &info );

	// Killed a character
	void InputKilledNPC( inputdata_t &inputdata );
#ifdef MAPBASE

	void InputGiveWeapon( inputdata_t &inputdata );
	void InputDropWeapon( inputdata_t &inputdata );
	void InputPickupWeaponInstant( inputdata_t &inputdata );
	COutputEvent	m_OnWeaponEquip;
	COutputEvent	m_OnWeaponDrop;

	virtual void	InputHolsterWeapon( inputdata_t &inputdata );
	virtual void	InputHolsterAndDestroyWeapon( inputdata_t &inputdata );
	virtual void	InputUnholsterWeapon( inputdata_t &inputdata );
	void			InputSwitchToWeapon( inputdata_t &inputdata );

	COutputEHANDLE	m_OnKilledEnemy;
	COutputEHANDLE	m_OnKilledPlayer;
	virtual void OnKilledNPC( CBaseCombatCharacter *pKilled ); 

	virtual	CBaseEntity *FindNamedEntity( const char *pszName, IEntityFindFilter *pFilter = NULL );

	COutputFloat	m_OnHealthChanged;
#else
	virtual void OnKilledNPC( CBaseCombatCharacter *pKilled ) {}; 
#endif

	// Exactly one of these happens immediately after killed (gibbed may happen later when the corpse gibs)
	// Character gibbed or faded out (violence controls) (only fired once)
	// returns true if gibs were spawned
	virtual bool			Event_Gibbed( const CTakeDamageInfo &info );
	// Character entered the dying state without being gibbed (only fired once)
	virtual void			Event_Dying( const CTakeDamageInfo &info );
	virtual void			Event_Dying();
	// character died and should become a ragdoll now
	// return true if converted to a ragdoll, false to use AI death
	virtual bool			BecomeRagdoll( const CTakeDamageInfo &info, const Vector &forceVector );
	virtual void			FixupBurningServerRagdoll( CBaseEntity *pRagdoll );

	virtual bool			BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags );

#ifdef MAPBASE
	// A version of BecomeRagdollBoogie() that allows the color to change and returns the entity itself instead.
	// In order to avoid breaking anything, it doesn't change the original function.
	virtual CBaseEntity		*BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags, const Vector *vecColor );
#endif

	CBaseEntity				*FindHealthItem( const Vector &vecPosition, const Vector &range );


	virtual CBaseEntity		*CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float forceScale = 1.0f, bool bDamageAnyNPC = false );
	virtual CBaseEntity		*CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float flForceScale = 1.0f, bool bDamageAnyNPC = false );

	virtual CBaseCombatCharacter *MyCombatCharacterPointer( void ) { return this; }

	// VPHYSICS
	virtual void			VPhysicsShadowCollision( int index, gamevcollisionevent_t *pEvent );
	virtual void			VPhysicsUpdate( IPhysicsObject *pPhysics );
	float					CalculatePhysicsStressDamage( vphysics_objectstress_t *pStressOut, IPhysicsObject *pPhysics );
	void					ApplyStressDamage( IPhysicsObject *pPhysics, bool bRequireLargeObject );

	virtual void			PushawayTouch( CBaseEntity *pOther ) {}

	void SetImpactEnergyScale( float fScale ) { m_impactEnergyScale = fScale; }

	virtual void			UpdateOnRemove( void );

	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );
	virtual int				IRelationPriority( CBaseEntity *pTarget );

#ifdef MAPBASE
	void					AddRelationship( const char *pszRelationship, CBaseEntity *pActivator );
	void					InputSetRelationship( inputdata_t &inputdata );
#endif

#ifdef EZ2
	// Blixibon - Used by the player's speech AI to prevent cases where enemies are picked up as D_FR
	// before it could possibly be apparent to the player
	virtual bool			JustStartedFearing( CBaseEntity *pTarget ) { return false; }

	virtual bool            TestRagdollPin( const Vector &vecOrigin, const Vector &vecDirection );
#endif

	virtual void			SetLightingOriginRelative( CBaseEntity *pLightingOrigin );

protected:
	Relationship_t			*FindEntityRelationship( CBaseEntity *pTarget );

public:
	
	// Vehicle queries
	virtual bool IsInAVehicle( void ) const { return false; }
	virtual IServerVehicle *GetVehicle( void ) { return NULL; }
	virtual CBaseEntity *GetVehicleEntity( void ) { return NULL; }
	virtual bool ExitVehicle( void ) { return false; }

	// Blood color (see BLOOD_COLOR_* macros in baseentity.h)
	void SetBloodColor( int nBloodColor );
#ifdef MAPBASE
	void InputSetBloodColor( inputdata_t &inputdata );
#endif

	// Weapons..
	CBaseCombatWeapon*	GetActiveWeapon() const;
	int					WeaponCount() const;
	CBaseCombatWeapon*	GetWeapon( int i ) const;
	bool				RemoveWeapon( CBaseCombatWeapon *pWeapon );
	virtual void		RemoveAllWeapons();
	WeaponProficiency_t GetCurrentWeaponProficiency()
	{
#ifdef MAPBASE
		// Mapbase adds proficiency override
		return (m_ProficiencyOverride > WEAPON_PROFICIENCY_INVALID) ? m_ProficiencyOverride : m_CurrentWeaponProficiency;
#else
		return m_CurrentWeaponProficiency;
#endif
	}
	void				SetCurrentWeaponProficiency( WeaponProficiency_t iProficiency ) { m_CurrentWeaponProficiency = iProficiency; }
	virtual WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );
#ifdef MAPBASE
	inline bool			OverridingWeaponProficiency() { return (m_ProficiencyOverride > WEAPON_PROFICIENCY_INVALID); }
#endif
	virtual	Vector		GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );
	virtual	float		GetSpreadBias(  CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget );
	virtual void		DoMuzzleFlash();

#ifdef MAPBASE_VSCRIPT
	HSCRIPT				ScriptGetActiveWeapon();
	HSCRIPT				ScriptGetWeapon( int i );
	HSCRIPT				ScriptGetWeaponByType( const char *pszWeapon, int iSubType = 0 );
	void				ScriptGetAllWeapons( HSCRIPT hTable );
	int					ScriptGetCurrentWeaponProficiency() { return GetCurrentWeaponProficiency(); }

	void				ScriptDropWeapon( HSCRIPT hWeapon );
	void				ScriptEquipWeapon( HSCRIPT hWeapon );
	
	int					ScriptGiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound = false );
	void				ScriptRemoveAmmo( int iCount, int iAmmoIndex );
	int					ScriptGetAmmoCount( int iType ) const;
	void				ScriptSetAmmoCount( int iType, int iCount );

	const Vector&		ScriptGetAttackSpread( HSCRIPT hWeapon, HSCRIPT hTarget );
	float				ScriptGetSpreadBias( HSCRIPT hWeapon, HSCRIPT hTarget );

	int					ScriptRelationType( HSCRIPT pTarget );
	int					ScriptRelationPriority( HSCRIPT pTarget );
	void				ScriptSetRelationship( HSCRIPT pTarget, int disposition, int priority );

	HSCRIPT				ScriptGetVehicleEntity();

	bool				ScriptInViewCone( const Vector &vecSpot ) { return FInViewCone( vecSpot ); }
	bool				ScriptEntInViewCone( HSCRIPT pEntity ) { return FInViewCone( ToEnt( pEntity ) ); }

	bool				ScriptInAimCone( const Vector &vecSpot ) { return FInAimCone( vecSpot ); }
	bool				ScriptEntInAimCone( HSCRIPT pEntity ) { return FInAimCone( ToEnt( pEntity ) ); }

	const Vector&		ScriptBodyAngles( void ) { static Vector vec; QAngle qa = BodyAngles(); vec.x = qa.x; vec.y = qa.y; vec.z = qa.z; return vec; }

	static ScriptHook_t		g_Hook_RelationshipType;
	static ScriptHook_t		g_Hook_RelationshipPriority;
#endif

	// Interactions
	static void			InitInteractionSystem();

	// Relationships
	static void			AllocateDefaultRelationships( );
	static void			SetDefaultRelationship( Class_T nClass, Class_T nClassTarget,  Disposition_t nDisposition, int nPriority );
#ifdef MAPBASE
	static Disposition_t	GetDefaultRelationshipDisposition( Class_T nClassSource, Class_T nClassTarget );
	static int				GetDefaultRelationshipPriority( Class_T nClassSource, Class_T nClassTarget );
	int						GetDefaultRelationshipPriority( Class_T nClassTarget );
#endif
	Disposition_t		GetDefaultRelationshipDisposition( Class_T nClassTarget );
	virtual void		AddEntityRelationship( CBaseEntity *pEntity, Disposition_t nDisposition, int nPriority );
	virtual bool		RemoveEntityRelationship( CBaseEntity *pEntity );
	virtual void		AddClassRelationship( Class_T nClass, Disposition_t nDisposition, int nPriority );
#ifdef MAPBASE
	virtual bool		RemoveClassRelationship( Class_T nClass );
#endif
#ifdef HE_APC
	//TERO: this one added by me
	static Disposition_t		GetDefaultRelationshipDispositionBetweenClasses( Class_T nClassTarget1, Class_T nClassTarget2 );
#endif

	virtual void		ChangeTeam( int iTeamNum );

	// Nav hull type
	Hull_t	GetHullType() const				{ return m_eHull; }
	void	SetHullType( Hull_t hullType )	{ m_eHull = hullType; }

	// FIXME: The following 3 methods are backdoor hack methods
	
	// This is a sort of hack back-door only used by physgun!
	void SetAmmoCount( int iCount, int iAmmoIndex );

	// This is a hack to blat out the current active weapon...
	// Used by weapon_slam + game_ui
	void SetActiveWeapon( CBaseCombatWeapon *pNewWeapon );
	void ClearActiveWeapon() { SetActiveWeapon( NULL ); }
	virtual void OnChangeActiveWeapon( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon ) {}
	virtual void OnPickupWeapon(CBaseCombatWeapon *pNewWeapon) {} // 1upD - stub for event when a weapon is picked up

	// I can't use my current weapon anymore. Switch me to the next best weapon.
	bool SwitchToNextBestWeapon(CBaseCombatWeapon *pCurrent);

	// This is a hack to copy the relationship strings used by monstermaker
	void SetRelationshipString( string_t theString ) { m_RelationshipString = theString; }

	float				GetNextAttack() const { return m_flNextAttack; }
	void				SetNextAttack( float flWait ) { m_flNextAttack = flWait; }

	bool				m_bForceServerRagdoll;

#ifdef EZ
	EHANDLE				m_hDeathRagdoll;
#endif

	// Pickup prevention
	bool				IsAllowedToPickupWeapons( void ) { return !m_bPreventWeaponPickup; }
	void				SetPreventWeaponPickup( bool bPrevent ) { m_bPreventWeaponPickup = bPrevent; }
	bool				m_bPreventWeaponPickup;

	virtual CNavArea *GetLastKnownArea( void ) const		{ return m_lastNavArea; }		// return the last nav area the player occupied - NULL if unknown
	virtual bool IsAreaTraversable( const CNavArea *area ) const;							// return true if we can use the given area 
	virtual void ClearLastKnownArea( void );
	virtual void UpdateLastKnownArea( void );										// invoke this to update our last known nav area (since there is no think method chained to CBaseCombatCharacter)
	virtual void OnNavAreaChanged( CNavArea *enteredArea, CNavArea *leftArea ) { }	// invoked (by UpdateLastKnownArea) when we enter a new nav area (or it is reset to NULL)
	virtual void OnNavAreaRemoved( CNavArea *removedArea );

	// -----------------------
	// Notification from INextBots.
	// -----------------------
	virtual void		OnPursuedBy( INextBot * RESTRICT pPursuer ){} // called every frame while pursued by a bot in DirectChase.

#ifdef GLOWS_ENABLE
	// Glows
	void				AddGlowEffect( void );
	void				RemoveGlowEffect( void );
	bool				IsGlowEffectActive( void );
	void				SetGlowColor( float red, float green, float blue, float alpha );
#endif // GLOWS_ENABLE

#ifdef INVASION_DLL
public:


	// TF2 Powerups
	virtual bool		CanBePoweredUp( void );
	bool				HasPowerup( int iPowerup );
	virtual	bool		CanPowerupNow( int iPowerup );		// Return true if I can be powered by this powerup right now
	virtual	bool		CanPowerupEver( int iPowerup );		// Return true if I ever accept this powerup type

	void				SetPowerup( int iPowerup, bool bState, float flTime = 0, float flAmount = 0, CBaseEntity *pAttacker = NULL, CDamageModifier *pDamageModifier = NULL );
	virtual	bool		AttemptToPowerup( int iPowerup, float flTime, float flAmount = 0, CBaseEntity *pAttacker = NULL, CDamageModifier *pDamageModifier = NULL );
	virtual	float		PowerupDuration( int iPowerup, float flTime );
	virtual	void		PowerupStart( int iPowerup, float flAmount = 0, CBaseEntity *pAttacker = NULL, CDamageModifier *pDamageModifier = NULL );
	virtual	void		PowerupEnd( int iPowerup );

	void				PowerupThink( void );
	virtual	void		PowerupThink( int iPowerup );

public:

	CNetworkVar( int, m_iPowerups );
	float				m_flPowerupAttemptTimes[ MAX_POWERUPS ];
	float				m_flPowerupEndTimes[ MAX_POWERUPS ];
	float				m_flFractionalBoost;	// POWERUP_BOOST health fraction - specific powerup data

#endif

public:
	// returns the last body region that took damage
	int	LastHitGroup() const				{ return m_LastHitGroup; }
#ifndef MAPBASE // For filter_damage_transfer
protected:
#endif
	void SetLastHitGroup( int nHitGroup )	{ m_LastHitGroup = nHitGroup; }

public:
	CNetworkVar( float, m_flNextAttack );			// cannot attack again until this time

#ifdef GLOWS_ENABLE
protected:
	CNetworkVar( bool, m_bGlowEnabled );
	CNetworkVector( m_GlowColor );
	CNetworkVar( float, m_GlowAlpha );
#endif // GLOWS_ENABLE

private:
	Hull_t		m_eHull;

	void				UpdateGlowEffect( void );
	void				DestroyGlowEffect( void );
protected:
	int			m_bloodColor;			// color of blood particless

	// -------------------
	// combat ability data
	// -------------------
	float		m_flFieldOfView;		// cosine of field of view for this character
	Vector		m_HackedGunPos;			// HACK until we can query end of gun
	string_t	m_RelationshipString;	// Used to load up relationship keyvalues
	float		m_impactEnergyScale;// scale the amount of energy used to calculate damage this ent takes due to physics

public:
	static int					GetInteractionID();	// Returns the next interaction #

#ifdef MAPBASE
	// Mapbase's new method for adding interactions which allows them to be handled with their names, currently for VScript
	static void					AddInteractionWithString( int &interaction, const char *szName );
#endif

protected:
	// Visibility-related stuff
	bool ComputeLOS( const Vector &vecEyePosition, const Vector &vecTarget ) const;
private:
	// For weapon strip
	void ThrowDirForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, Vector *pVecThrowDir );
	void DropWeaponForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, const QAngle &vecAngles, float flDiameter );

	friend class CScriptedTarget; // needs to access GetInteractionID()
	
	static int					m_lastInteraction;	// Last registered interaction #
	static Relationship_t**		m_DefaultRelationship;

	// attack/damage
	int					m_LastHitGroup;			// the last body region that took damage
	float				m_flDamageAccumulator;	// so very small amounts of damage do not get lost.
	int					m_iDamageCount;			// # of times NPC has been damaged.  used for tracking 1-shot kills.
	
	// Weapon proficiency gets calculated each time an NPC changes his weapon, and then
	// cached off as the CurrentWeaponProficiency.
	WeaponProficiency_t m_CurrentWeaponProficiency;

#ifdef MAPBASE
	// Weapon proficiency can be overridden with this.
	WeaponProficiency_t m_ProficiencyOverride = WEAPON_PROFICIENCY_INVALID;
#endif

	// ---------------
	//  Relationships
	// ---------------
	CUtlVector<Relationship_t>		m_Relationship;						// Array of relationships

protected:
	// shared ammo slots
	CNetworkArrayForDerived( int, m_iAmmo, MAX_AMMO_SLOTS );

	// Usable character items 
	CNetworkArray( CBaseCombatWeaponHandle, m_hMyWeapons, MAX_WEAPONS );

	CNetworkHandle( CBaseCombatWeapon, m_hActiveWeapon );

	friend class CCleanupDefaultRelationShips;
	
	IntervalTimer m_aliveTimer;

	unsigned int m_hasBeenInjured;							// bitfield corresponding to team ID that did the injury	

	// we do this because MAX_TEAMS is 32, which is wasteful for most games
	enum { MAX_DAMAGE_TEAMS = 4 };
	struct DamageHistory
	{
		int team;					// which team hurt us (TEAM_INVALID means slot unused)
		IntervalTimer interval;		// how long has it been
	};
	DamageHistory m_damageHistory[ MAX_DAMAGE_TEAMS ];

	// last known navigation area of player - NULL if unknown
	CNavArea *m_lastNavArea;
	CAI_MoveMonitor m_NavAreaUpdateMonitor;
	int m_registeredNavTeam;	// ugly, but needed to clean up player team counts in nav mesh
};


inline float CBaseCombatCharacter::GetAliveDuration( void ) const
{
	return m_aliveTimer.GetElapsedTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int	CBaseCombatCharacter::WeaponCount() const
{
	return MAX_WEAPONS;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
//-----------------------------------------------------------------------------
inline CBaseCombatWeapon *CBaseCombatCharacter::GetWeapon( int i ) const
{
	Assert( (i >= 0) && (i < MAX_WEAPONS) );
	return m_hMyWeapons[i].Get();
}

#ifdef INVASION_DLL
// Powerup Inlines
inline bool CBaseCombatCharacter::CanBePoweredUp( void )							{ return true; }
inline float CBaseCombatCharacter::PowerupDuration( int iPowerup, float flTime )	{ return flTime; }
inline void	CBaseCombatCharacter::PowerupEnd( int iPowerup )						{ return; }
inline void	CBaseCombatCharacter::PowerupThink( int iPowerup )						{ return; }
#endif

EXTERN_SEND_TABLE(DT_BaseCombatCharacter);

void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTraceFilterMelee : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterMelee );
	
	CTraceFilterMelee( const IHandleEntity *passentity, int collisionGroup, CTakeDamageInfo *dmgInfo, float flForceScale, bool bDamageAnyNPC )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_dmgInfo(dmgInfo), m_pHit(NULL), m_flForceScale(flForceScale), m_bDamageAnyNPC(bDamageAnyNPC)
	{
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CTakeDamageInfo		*m_dmgInfo;
	CBaseEntity			*m_pHit;
	float				m_flForceScale;
	bool				m_bDamageAnyNPC;
};

#endif // BASECOMBATCHARACTER_H

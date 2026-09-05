//========= Merged: our fixes + additional public-SDK techniques =========
//
// prop_weighted_cube, CubeType-driven model/skin switch.
//
// NOTE ON MODEL PATHS: none of the model paths below are verified against a
// real Portal 2 install. "models/props/metal_box.mdl" is a REAL path but is
// the ordinary HL2 wooden/metal crate, not the actual Portal cube -- it's
// only here as a placeholder so precache/spawn don't fail outright. Check
// your own legally obtained Portal 2 files for the real per-CubeType model
// paths and swap them in below.
//
//===========================================================================

#include "cbase.h"
#include "props.h"
#include "tier0/memdbgon.h" // must be last include

enum WeightedCubeType_e
{
	CUBE_STANDARD = 0,
	CUBE_COMPANION = 1,
	CUBE_REFLECTIVE = 2,
	CUBE_SPHERE = 3,
	CUBE_ANTIQUE = 4,
	CUBE_SCHRODINGER = 5
};

class CPropWeightedCube : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropWeightedCube, CPhysicsProp );
	DECLARE_DATADESC();

	void			Precache( void );
	void			Spawn( void );
	virtual int		ObjectCaps( void );

private:
	int		m_nCubeType;
};

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),
END_DATADESC()

int CPropWeightedCube::ObjectCaps( void )
{
	// Allow bare-handed +use pickup, not just physcannon.
	return ( BaseClass::ObjectCaps() | FCAP_IMPULSE_USE );
}

void CPropWeightedCube::Precache( void )
{
	// PLACEHOLDER PATHS -- see note at top of file.
	PrecacheModel( "models/props/metal_box.mdl" );
	PrecacheModel( "models/props/reflection_cube.mdl" );
	PrecacheModel( "models/props_gameplay/mp_ball.mdl" );
	PrecacheModel( "models/props_underground/underground_weighted_cube.mdl" );

	BaseClass::Precache();
}

void CPropWeightedCube::Spawn( void )
{
	Precache();

	const char *pszModel = "models/props/metal_box.mdl";
	int nTargetSkin = 0;

	switch ( m_nCubeType )
	{
		case CUBE_COMPANION:
			pszModel = "models/props/metal_box.mdl";
			nTargetSkin = 1;
			break;
		case CUBE_REFLECTIVE:
		case CUBE_SCHRODINGER:
			pszModel = "models/props/reflection_cube.mdl";
			nTargetSkin = 0;
			break;
		case CUBE_SPHERE:
			pszModel = "models/props_gameplay/mp_ball.mdl";
			nTargetSkin = 0;
			break;
		case CUBE_ANTIQUE:
			pszModel = "models/props_underground/underground_weighted_cube.mdl";
			nTargetSkin = 0;
			break;
		case CUBE_STANDARD:
		default:
			pszModel = "models/props/metal_box.mdl";
			nTargetSkin = 0;
			break;
	}

	SetModelName( MAKE_STRING( pszModel ) );
	m_nSkin = nTargetSkin;

	// Without this flag the physcannon does not register the prop as an
	// official pickup target (confirmed by cross-checking Portal 1's own
	// weight box spawn command, CC_Create_PortalWeightBox, which sets this
	// exact flag on the plain prop_physics it creates).
	AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );

	BaseClass::Spawn();

	// Documented flag allowing the physcannon to hold this prop overhead.
	SetInteraction( PROPINTER_PHYSGUN_ALLOW_OVERHEAD );

	// Force-wake the physics object in case it spawned asleep.
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->Wake();
	}

	// TODO once model paths are verified and cube spawns correctly: add
	// laser reflection for CUBE_REFLECTIVE, companion cube "sad death"
	// particle/sound on destruction, and fizzler-triggered respawn logic.
}

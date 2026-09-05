//========= Minimal skeleton, built on public Source SDK 2013 CPhysicsProp =========
//
// Step 1 goal: cube spawns, falls under gravity, can be picked up/thrown by
// the physcannon (inherited for free from CPhysicsProp). No portal-specific
// button/laser reactivity yet -- add that once this compiles and works.
//
//====================================================================================

#include "cbase.h"
#include "props.h"          // CPhysicsProp lives here in stock Source SDK 2013
#include "tier0/memdbgon.h" // must be last include

//-----------------------------------------------------------------------------
// Purpose: Portal 2's weighted storage cube. For now this is just a
// CPhysicsProp with a different classname and a couple of Portal-flavoured
// spawnflags stubbed in for later use (laser catcher reactivity, etc).
//-----------------------------------------------------------------------------
class CPropWeightedCube : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropWeightedCube, CPhysicsProp );
	DECLARE_DATADESC();

	void	Spawn( void );

	// Placeholder for later: laser-reflective "Discouragement Redirection Cube"
	// variant, Fizzler-related cleanup, respawn-on-fizzle behavior, etc.
	// None of that is implemented yet -- this only gets the entity to exist
	// and behave like a normal physics prop.

private:
	int		m_nCubeType; // 0 = standard, 1 = companion, 2 = reflective, 3 = sphere (unused for now)
};

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),
END_DATADESC()

void CPropWeightedCube::Spawn( void )
{
	// Without this flag the physcannon does not register the prop as an
	// official pickup target (confirmed by cross-checking Portal 1's own
	// weight box spawn command, CC_Create_PortalWeightBox, which sets this
	// exact flag on the plain prop_physics it creates). The object is still
	// physically simulated without it, but +use/physcannon pickup silently
	// does nothing -- which matches exactly what was being seen before this
	// fix.
	AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );

	// Reuse all of CPhysicsProp's normal physics-prop spawn behavior
	// (model precache, VPhysics object creation, interaction as a physics
	// object, physcannon pickup support, etc.) -- we get this for free.
	BaseClass::Spawn();

	// TODO once this compiles and the cube visibly spawns + falls + can be
	// picked up: add laser reflection for CubeType==2, companion cube
	// "sad death" particle/sound on destruction for CubeType==1, and
	// fizzler-triggered respawn logic (needs a trigger_portal_cleanser hook,
	// which is a separate entity you'd need to check exists in your tree).
}

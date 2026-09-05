#include "cbase.h"
#include "props.h"
#include "tier0/memdbgon.h"

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

	void Spawn( void );
	void Precache( void );
	virtual int ObjectCaps( void ); // Додаємо капу для підняття руками

private:
	int m_nCubeType;
};

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),
END_DATADESC()

int CPropWeightedCube::ObjectCaps( void )
{
	// Дозволяємо підбирати проп кнопкою +USE (голими руками)
	return ( BaseClass::ObjectCaps() | FCAP_IMPULSE_USE );
}

void CPropWeightedCube::Precache( void )
{
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

	BaseClass::Spawn();

	// Дозволяємо взаємодію з фізганом / портальною гарматою
	SetInteraction( PROPINTER_PHYSGUN_ALLOW_OVERHEAD );

	// Примусово прокидаємо фізичний об'єкт, щоб він не був "замороженим"
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->Wake();
	}
}

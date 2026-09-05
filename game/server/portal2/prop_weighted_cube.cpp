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

private:
	int m_nCubeType;
};

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),
END_DATADESC()

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

	// Спочатку виставляємо модель і скін ДО BaseClass::Spawn(), 
	// щоб сорсівський CPhysicsProp одразу створив правильну фізику та коллбокси
	switch ( m_nCubeType )
	{
		case CUBE_COMPANION:
			SetModelName( MAKE_STRING( "models/props/metal_box.mdl" ) );
			m_nSkin = 1;
			break;
		case CUBE_REFLECTIVE:
		case CUBE_SCHRODINGER:
			SetModelName( MAKE_STRING( "models/props/reflection_cube.mdl" ) );
			m_nSkin = 0;
			break;
		case CUBE_SPHERE:
			SetModelName( MAKE_STRING( "models/props_gameplay/mp_ball.mdl" ) );
			m_nSkin = 0;
			break;
		case CUBE_ANTIQUE:
			SetModelName( MAKE_STRING( "models/props_underground/underground_weighted_cube.mdl" ) );
			m_nSkin = 0;
			break;
		case CUBE_STANDARD:
		default:
			SetModelName( MAKE_STRING( "models/props/metal_box.mdl" ) );
			m_nSkin = 0;
			break;
	}

	// Тепер стандартний спавн усе підхопить сам ідеально
	BaseClass::Spawn();
}

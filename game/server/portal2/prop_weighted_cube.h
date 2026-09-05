//========= Shared header for cross-file CubeType access =========
#ifndef PROP_WEIGHTED_CUBE_H
#define PROP_WEIGHTED_CUBE_H

#include "props.h"

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

	WeightedCubeType_e GetCubeType( void ) const { return (WeightedCubeType_e)m_nCubeType; }

private:
	int		m_nCubeType;
};

#endif // PROP_WEIGHTED_CUBE_H

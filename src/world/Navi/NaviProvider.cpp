#include <Common.h>
#include <Framework.h>
#include <Territory/Zone.h>
#include <Logging/Logger.h>
#include <ServerMgr.h>


#include "NaviProvider.h"

#include <recastnavigation/Detour/Include/DetourNavMesh.h>
#include <recastnavigation/Detour/Include/DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <recastnavigation/Recast/Include/Recast.h>
#include <experimental/filesystem>

Sapphire::World::Navi::NaviProvider::NaviProvider( const std::string& internalName, FrameworkPtr pFw ) :
  m_naviMesh( nullptr ),
  m_naviMeshQuery( nullptr ),
  m_internalName( internalName ),
  m_pFw( pFw )
{
  // Set defaults
  m_polyFindRange[ 0 ] = 10;
  m_polyFindRange[ 1 ] = 20;
  m_polyFindRange[ 2 ] = 10;
}

bool Sapphire::World::Navi::NaviProvider::init()
{
  auto& cfg = m_pFw->get< Sapphire::World::ServerMgr >()->getConfig();

  auto meshesFolder = std::experimental::filesystem::path( cfg.navigation.meshPath );
  auto meshFolder = meshesFolder / std::experimental::filesystem::path( m_internalName );

  if( std::experimental::filesystem::exists( meshFolder ) )
  {
    auto baseMesh = meshFolder / std::experimental::filesystem::path( m_internalName + ".nav" );

    loadMesh( baseMesh.string() );

    initQuery();

    return true;
  }

  return false;
}

bool Sapphire::World::Navi::NaviProvider::hasNaviMesh() const
{
  return m_naviMesh != nullptr;
}

void Sapphire::World::Navi::NaviProvider::initQuery()
{
  if( m_naviMeshQuery )
    dtFreeNavMeshQuery( m_naviMeshQuery );

  m_naviMeshQuery = dtAllocNavMeshQuery();
  m_naviMeshQuery->init( m_naviMesh, 2048 );
}

int32_t Sapphire::World::Navi::NaviProvider::fixupCorridor( dtPolyRef* path, const int32_t npath, const int32_t maxPath,
                                                            const dtPolyRef* visited, const int32_t nvisited )
{
  int32_t furthestPath = -1;
  int32_t furthestVisited = -1;

  // Find furthest common polygon.
  for( int32_t i = npath - 1; i >= 0; --i )
  {
    bool found = false;
    for( int32_t j = nvisited - 1; j >= 0; --j )
    {
      if( path[ i ] == visited[ j ] )
      {
        furthestPath = i;
        furthestVisited = j;
        found = true;
      }
    }
    if( found )
      break;
  }

  // If no intersection found just return current path. 
  if( furthestPath == -1 || furthestVisited == -1 )
    return npath;

  // Concatenate paths.	

  // Adjust beginning of the buffer to include the visited.
  const int32_t req = nvisited - furthestVisited;
  const int32_t orig = rcMin( furthestPath + 1, npath );
  int32_t size = rcMax( 0, npath - orig );
  if( req + size > maxPath )
    size = maxPath - req;
  if( size )
    memmove( path + req, path + orig, size * sizeof( dtPolyRef ) );

  // Store visited
  for( int32_t i = 0; i < req; ++i )
    path[i] = visited[( nvisited - 1 ) - i];

  return req + size;
}

int32_t Sapphire::World::Navi::NaviProvider::fixupShortcuts( dtPolyRef* path, int32_t npath, dtNavMeshQuery* navQuery )
{
  if( npath < 3 )
    return npath;

  // Get connected polygons
  const int32_t maxNeis = 16;
  dtPolyRef neis[ maxNeis ];
  int32_t nneis = 0;

  const dtMeshTile* tile = 0;
  const dtPoly* poly = 0;
  if( dtStatusFailed( navQuery->getAttachedNavMesh()->getTileAndPolyByRef( path[ 0 ], &tile, &poly ) ) )
    return npath;

  for( uint32_t k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[ k ].next )
  {
    const dtLink* link = &tile->links[ k ];
    if( link->ref != 0 )
    {
      if( nneis < maxNeis )
        neis[ nneis++ ] = link->ref;
    }
  }

  // If any of the neighbour polygons is within the next few polygons
  // in the path, short cut to that polygon directly.
  const int32_t maxLookAhead = 6;
  int32_t cut = 0;
  for( int32_t i = dtMin( maxLookAhead, npath ) - 1; i > 1 && cut == 0; i-- ) 
  {
    for( int32_t j = 0; j < nneis; j++ )
    {
      if( path[ i ] == neis[ j ] ) 
      {
        cut = i;
        break;
      }
    }
  }
  if( cut > 1 )
  {
    int32_t offset = cut - 1;
    npath -= offset;
    for( int32_t i = 1; i < npath; i++ )
      path[ i ] = path[ i + offset ];
  }

  return npath;
}

bool Sapphire::World::Navi::NaviProvider::inRange( const float* v1, const float* v2, const float r, const float h )
{
  const float dx = v2[ 0 ] - v1[ 0 ];
  const float dy = v2[ 1 ] - v1[ 1 ];
  const float dz = v2[ 2 ] - v1[ 2 ];
  return ( dx * dx + dz * dz ) < r * r && fabsf( dy ) < h;
}

bool Sapphire::World::Navi::NaviProvider::getSteerTarget( dtNavMeshQuery* navQuery, const float* startPos, const float* endPos,
                                                          const float minTargetDist, const dtPolyRef* path, const int32_t pathSize,
                                                          float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef,
                                                          float* outPoints, int32_t* outPointCount )
{
  // Find steer target.
  const int32_t MAX_STEER_POINTS = 3;
  float steerPath[ MAX_STEER_POINTS * 3 ];
  uint8_t steerPathFlags[ MAX_STEER_POINTS ];
  dtPolyRef steerPathPolys[ MAX_STEER_POINTS ];
  int32_t nsteerPath = 0;
  navQuery->findStraightPath( startPos, endPos, path, pathSize,
                              steerPath, steerPathFlags, steerPathPolys, &nsteerPath, MAX_STEER_POINTS );
  if( !nsteerPath )
    return false;

  if( outPoints && outPointCount )
  {
    *outPointCount = nsteerPath;
    for( int32_t i = 0; i < nsteerPath; ++i )
      dtVcopy( &outPoints[ i * 3 ], &steerPath[ i * 3 ] );
  }

  // Find vertex far enough to steer to.
  int32_t ns = 0;
  while( ns < nsteerPath )
  {
    // Stop at Off-Mesh link or when point is further than slop away.
    if( ( steerPathFlags[ ns ] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ||
        !inRange( &steerPath[ ns * 3 ], startPos, minTargetDist, 1000.0f ) )
      break;
    ns++;
  }
  // Failed to find good point to steer to.
  if( ns >= nsteerPath )
    return false;

  dtVcopy( steerPos, &steerPath[ ns * 3 ] );
  steerPos[ 1 ] = startPos[ 1 ];
  steerPosFlag = steerPathFlags[ ns ];
  steerPosRef = steerPathPolys[ ns ];

  return true;
}

std::vector< Sapphire::Common::FFXIVARR_POSITION3 > 
  Sapphire::World::Navi::NaviProvider::findFollowPath( const Common::FFXIVARR_POSITION3& startPos,
                                                       const Common::FFXIVARR_POSITION3& endPos )
{
  if( !m_naviMesh || !m_naviMeshQuery )
    throw std::runtime_error( "No navimesh loaded" );

  auto resultCoords = std::vector< Common::FFXIVARR_POSITION3 >();

  dtPolyRef startRef, endRef = 0;

  float spos[ 3 ] = { startPos.x, startPos.y, startPos.z };
  float epos[ 3 ] = { endPos.x, endPos.y, endPos.z };

  dtQueryFilter filter;
  filter.setIncludeFlags( 0xffff );
  filter.setExcludeFlags( 0 );

  m_naviMeshQuery->findNearestPoly( spos, m_polyFindRange, &filter, &startRef, 0 );
  m_naviMeshQuery->findNearestPoly( epos, m_polyFindRange, &filter, &endRef, 0 );

  // Couldn't find any close polys to navigate from
  if( !startRef || !endRef )
    return resultCoords;

  dtPolyRef polys[ MAX_POLYS ];
  int32_t numPolys = 0;

  m_naviMeshQuery->findPath( startRef, endRef, spos, epos, &filter, polys, &numPolys, MAX_POLYS );

  // Check if we got polys back for navigation
  if( numPolys )
  {
    // Iterate over the path to find smooth path on the detail mesh surface.
    memcpy( polys, polys, sizeof( dtPolyRef )*numPolys );
    int32_t npolys = numPolys;

    float iterPos[3], targetPos[3];
    m_naviMeshQuery->closestPointOnPoly( startRef, spos, iterPos, 0 );
    m_naviMeshQuery->closestPointOnPoly( polys[ npolys - 1 ], epos, targetPos, 0 );

    Logger::debug( "IterPos: {0} {1} {2}; TargetPos: {3} {4} {5}",
                   iterPos[ 0 ], iterPos[ 1 ], iterPos[ 2 ],
                   targetPos[ 0 ], targetPos[ 1 ], targetPos[ 2 ] );

    const float STEP_SIZE = 1.2f;
    const float SLOP = 0.15f;

    int32_t numSmoothPath = 0;
    float smoothPath[ MAX_SMOOTH * 3 ];

    dtVcopy( &smoothPath[ numSmoothPath * 3 ], iterPos );
    numSmoothPath++;

    // Move towards target a small advancement at a time until target reached or
    // when ran out of memory to store the path.
    while( npolys && numSmoothPath < MAX_SMOOTH )
    {
      // Find location to steer towards.
      float steerPos[ 3 ];
      uint8_t steerPosFlag;
      dtPolyRef steerPosRef;

      if( !getSteerTarget( m_naviMeshQuery, iterPos, targetPos, SLOP,
                           polys, npolys, steerPos, steerPosFlag, steerPosRef ) )
        break;

      bool endOfPath = ( steerPosFlag & DT_STRAIGHTPATH_END ) ? true : false;
      bool offMeshConnection = ( steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ? true : false;

      // Find movement delta.
      float delta[ 3 ], len;
      dtVsub( delta, steerPos, iterPos );
      len = dtMathSqrtf( dtVdot( delta, delta ) );
      // If the steer target is end of path or off-mesh link, do not move past the location.
      if( ( endOfPath || offMeshConnection ) && len < STEP_SIZE )
        len = 1;
      else
        len = STEP_SIZE / len;
      float moveTgt[ 3 ];
      dtVmad( moveTgt, iterPos, delta, len );

      // Move
      float result[ 3 ];
      dtPolyRef visited[ 16 ];
      int32_t nvisited = 0;
      m_naviMeshQuery->moveAlongSurface( polys[ 0 ], iterPos, moveTgt, &filter,
                                         result, visited, &nvisited, 16 );

      npolys = fixupCorridor( polys, npolys, MAX_POLYS, visited, nvisited );
      npolys = fixupShortcuts( polys, npolys, m_naviMeshQuery );

      float h = 0;
      m_naviMeshQuery->getPolyHeight( polys[0], result, &h );
      result[ 1 ] = h;
      dtVcopy( iterPos, result );

      // Handle end of path and off-mesh links when close enough.
      if( endOfPath && inRange( iterPos, steerPos, SLOP, 1.0f ) )
      {
        // Reached end of path.
        dtVcopy( iterPos, targetPos );
        if( numSmoothPath < MAX_SMOOTH )
        {
          dtVcopy( &smoothPath[ numSmoothPath * 3 ], iterPos );
          numSmoothPath++;
        }
        break;
      }
      else if( offMeshConnection && inRange( iterPos, steerPos, SLOP, 1.0f ) )
      {
        // Reached off-mesh connection.
        float startPos[ 3 ], endPos[ 3 ];

        // Advance the path up to and over the off-mesh connection.
        dtPolyRef prevRef = 0, polyRef = polys[ 0 ];
        int32_t npos = 0;
        while( npos < npolys && polyRef != steerPosRef )
        {
          prevRef = polyRef;
          polyRef = polys[ npos ];
          npos++;
        }
        for( int32_t i = npos; i < npolys; ++i )
          polys[ i - npos ] = polys[ i ];
        npolys -= npos;

        // Handle the connection.
        dtStatus status = m_naviMesh->getOffMeshConnectionPolyEndPoints( prevRef, polyRef, startPos, endPos );
        if( dtStatusSucceed( status ) )
        {
          if( numSmoothPath < MAX_SMOOTH )
          {
            dtVcopy( &smoothPath[ numSmoothPath * 3 ], startPos );
            numSmoothPath++;
            // Hack to make the dotted path not visible during off-mesh connection.
            if( numSmoothPath & 1 )
            {
              dtVcopy( &smoothPath[ numSmoothPath * 3 ], startPos );
              numSmoothPath++;
            }
          }
          // Move position at the other side of the off-mesh link.
          dtVcopy( iterPos, endPos );
          float eh = 0.0f;
          m_naviMeshQuery->getPolyHeight( polys[ 0 ], iterPos, &eh );
          iterPos[ 1 ] = eh;
        }
      }

      // Store results.
      if( numSmoothPath < MAX_SMOOTH )
      {
        dtVcopy( &smoothPath[ numSmoothPath * 3 ], iterPos );
        numSmoothPath++;
      }
    }

    for( int32_t i = 0; i < numSmoothPath; i += 3 )
    {
      resultCoords.push_back( Common::FFXIVARR_POSITION3{ smoothPath[ i ], smoothPath[ i + 1 ], smoothPath[ i + 2 ] } );
    }
  }

  return resultCoords;
}

void Sapphire::World::Navi::NaviProvider::loadMesh( const std::string& path )
{
  FILE* fp = fopen( path.c_str(), "rb" );
  if( !fp )
    throw std::runtime_error( "Could open navimesh file" );

  // Read header.
  NavMeshSetHeader header;

  size_t readLen = fread( &header, sizeof( NavMeshSetHeader ), 1, fp );
  if( readLen != 1 )
  {
    fclose( fp );
    throw std::runtime_error( "Could not read NavMeshSetHeader" );
  }

  if( header.magic != NAVMESHSET_MAGIC )
  {
    fclose( fp );
    throw std::runtime_error( "Not a NavMeshSet" );
  }

  if( header.version != NAVMESHSET_VERSION )
  {
    fclose( fp );
    throw std::runtime_error( "Invalid NavMeshSet version" );
  }

  if( !m_naviMesh )
  {
    m_naviMesh = dtAllocNavMesh();
    if( !m_naviMesh )
    {
      fclose( fp );
      throw std::runtime_error( "Could not allocate dtNavMesh" );
    }

    dtStatus status = m_naviMesh->init( &header.params );
    if( dtStatusFailed( status ) )
    {
      fclose( fp );
      throw std::runtime_error( "Could not initialize dtNavMesh" );
    }
  }

  // Read tiles.
  for( int32_t i = 0; i < header.numTiles; ++i )
  {
    NavMeshTileHeader tileHeader;
    readLen = fread( &tileHeader, sizeof( tileHeader ), 1, fp );
    if( readLen != 1 )
    {
      fclose( fp );
      throw std::runtime_error( "Could not read NavMeshTileHeader" );
    }

    if( !tileHeader.tileRef || !tileHeader.dataSize )
      break;

    uint8_t* data = reinterpret_cast< uint8_t* >( dtAlloc( tileHeader.dataSize, DT_ALLOC_PERM ) );
    if( !data ) 
      break;
    memset( data, 0, tileHeader.dataSize );
    readLen = fread( data, tileHeader.dataSize, 1, fp );
    if( readLen != 1 )
    {
      dtFree( data );
      fclose( fp );
      throw std::runtime_error( "Could not read tile data" );
    }

    m_naviMesh->addTile( data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0 );
  }

  fclose( fp );
}

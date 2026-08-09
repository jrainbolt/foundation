#include <foundation/world.h>

#include "world_internal.h"

#include <limits.h>
#include <stdlib.h>

#define CHANNEL_WATER UINT64_C(0x7761746572)
#define CHANNEL_ROCK UINT64_C(0x726f636b)
#define CHANNEL_PATCH UINT64_C(0x7061746368)

static uint64_t splitmix64(uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

static uint64_t spatial_hash(FactoryWorldSeed seed,uint32_t x,uint32_t y,
    uint64_t channel)
{
    return splitmix64(seed ^ channel
        ^ ((uint64_t)x * UINT64_C(0x9e3779b185ebca87))
        ^ ((uint64_t)y * UINT64_C(0xc2b2ae3d27d4eb4f)));
}

static uint16_t field_value(FactoryWorldSeed seed,uint32_t x,uint32_t y,
    uint32_t scale,uint64_t channel)
{
    uint32_t gx=x/scale,gy=y/scale,fx=x%scale,fy=y%scale;
    uint64_t a=spatial_hash(seed,gx,gy,channel)&UINT16_MAX;
    uint64_t b=spatial_hash(seed,gx+1U,gy,channel)&UINT16_MAX;
    uint64_t c=spatial_hash(seed,gx,gy+1U,channel)&UINT16_MAX;
    uint64_t d=spatial_hash(seed,gx+1U,gy+1U,channel)&UINT16_MAX;
    uint64_t top=a*(scale-fx)+b*fx;
    uint64_t bottom=c*(scale-fx)+d*fx;
    return (uint16_t)((top*(scale-fy)+bottom*fy)/((uint64_t)scale*scale));
}

void factory_world_generation_default_config(FactoryWorldGenerationConfig *out)
{
    if(out!=NULL)*out=(FactoryWorldGenerationConfig){
        5U,6U,10500U,7500U,2U,8U,500U,4U,3U,900U,35U};
}

int32_t factory_world_get_start_x(const FactoryWorld *world)
{return world==NULL?0:(int32_t)(world->width/2U);}
int32_t factory_world_get_start_y(const FactoryWorld *world)
{return world==NULL?0:(int32_t)(world->height/2U);}

static bool config_valid(const FactoryWorldGenerationConfig *c)
{
    return c!=NULL&&c->starting_area_radius>0U&&c->terrain_scale>0U
        &&c->terrain_scale<=64U&&c->starting_area_radius<=32767U
        &&c->starter_patch_radius>0U&&c->starter_patch_radius<=32767U
        &&c->starter_distance>c->starting_area_radius
        &&c->starter_quantity>0U&&c->remote_patch_radius>0U
        &&c->remote_patch_radius<=32767U&&c->remote_patch_count<=1024U
        &&c->remote_base_quantity>0U;
}

static size_t tile_index(const FactoryWorld *world,uint32_t x,uint32_t y)
{return (size_t)y*(size_t)world->width+x;}

static void clear_path(FactoryWorld *world,int32_t x0,int32_t y0,
    int32_t x1,int32_t y1)
{
    while(x0!=x1){world->tiles[tile_index(world,(uint32_t)x0,(uint32_t)y0)].terrain=FACTORY_TERRAIN_GROUND;x0+=x0<x1?1:-1;}
    while(y0!=y1){world->tiles[tile_index(world,(uint32_t)x0,(uint32_t)y0)].terrain=FACTORY_TERRAIN_GROUND;y0+=y0<y1?1:-1;}
    world->tiles[tile_index(world,(uint32_t)x1,(uint32_t)y1)].terrain=FACTORY_TERRAIN_GROUND;
}

static uint32_t place_patch(FactoryWorld *world,int32_t cx,int32_t cy,
    uint32_t radius,FactoryResourceType resource,uint32_t quantity,
    uint64_t salt)
{
    uint32_t placed=0U;
    int32_t r=(int32_t)radius;
    for(int32_t dy=-r;dy<=r;++dy)for(int32_t dx=-r;dx<=r;++dx){
        int32_t x=cx+dx,y=cy+dy;
        uint32_t d2=(uint32_t)(dx*dx+dy*dy);
        uint32_t edge=(uint32_t)(spatial_hash(world->seed,(uint32_t)x,
            (uint32_t)y,salt)%((uint64_t)radius+1U));
        FactoryTile *tile;
        uint64_t varied;
        if(!factory_world_is_in_bounds(world,x,y)
            ||d2>radius*radius+edge||d2>(radius+1U)*(radius+1U))continue;
        tile=&world->tiles[tile_index(world,(uint32_t)x,(uint32_t)y)];
        if(tile->resource!=FACTORY_RESOURCE_NONE)continue;
        varied=(uint64_t)quantity+(spatial_hash(world->seed,(uint32_t)x,
            (uint32_t)y,salt^UINT64_C(0x51))%(quantity/4U+1U));
        if(varied>FACTORY_RESOURCE_QUANTITY_MAX)varied=FACTORY_RESOURCE_QUANTITY_MAX;
        tile->terrain=FACTORY_TERRAIN_GROUND;
        tile->resource=resource;tile->resource_amount=(uint32_t)varied;
        ++placed;
    }
    return placed;
}

static bool center_fits(const FactoryWorld *world,int32_t x,int32_t y,
    uint32_t radius)
{
    return x>=(int32_t)radius&&y>=(int32_t)radius
        &&(uint64_t)(uint32_t)x+radius<world->width
        &&(uint64_t)(uint32_t)y+radius<world->height;
}

static bool place_starters(FactoryWorld *world,
    const FactoryWorldGenerationConfig *c)
{
    static const int32_t directions[8][2]={
        {1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    int32_t sx=factory_world_get_start_x(world),sy=factory_world_get_start_y(world);
    uint32_t first=(uint32_t)(spatial_hash(world->seed,0U,0U,CHANNEL_PATCH)%8U);
    uint32_t second=(first+3U+(uint32_t)(spatial_hash(
        world->seed,1U,0U,CHANNEL_PATCH)%3U))%8U;
    int32_t ix=sx+directions[first][0]*(int32_t)c->starter_distance;
    int32_t iy=sy+directions[first][1]*(int32_t)c->starter_distance;
    int32_t cx=sx+directions[second][0]*(int32_t)c->starter_distance;
    int32_t cy=sy+directions[second][1]*(int32_t)c->starter_distance;
    if(!center_fits(world,ix,iy,c->starter_patch_radius)
        ||!center_fits(world,cx,cy,c->starter_patch_radius))return false;
    clear_path(world,sx,sy,ix,iy);clear_path(world,sx,sy,cx,cy);
    return place_patch(world,ix,iy,c->starter_patch_radius,
        FACTORY_RESOURCE_IRON,c->starter_quantity,CHANNEL_PATCH|1U)>=3U
        &&place_patch(world,cx,cy,c->starter_patch_radius,
        FACTORY_RESOURCE_COPPER,c->starter_quantity,CHANNEL_PATCH|2U)>=3U;
}

static void place_remote_patches(FactoryWorld *world,
    const FactoryWorldGenerationConfig *c)
{
    int32_t sx=factory_world_get_start_x(world),sy=factory_world_get_start_y(world);
    uint32_t placed=0U;
    for(uint32_t attempt=0U;attempt<c->remote_patch_count*32U&&placed<c->remote_patch_count;++attempt){
        uint64_t h=spatial_hash(world->seed,attempt,placed,CHANNEL_PATCH|4U);
        int32_t x=(int32_t)(h%world->width);
        int32_t y=(int32_t)((h>>32U)%world->height);
        uint32_t distance=(uint32_t)(abs(x-sx)+abs(y-sy));
        FactoryResourceType type=(placed&1U)==0U?FACTORY_RESOURCE_IRON:FACTORY_RESOURCE_COPPER;
        uint64_t quantity=(uint64_t)c->remote_base_quantity
            +(uint64_t)(distance/(c->starting_area_radius+1U))*c->remote_distance_bonus;
        if(distance<=c->starter_distance+c->remote_patch_radius
            ||!center_fits(world,x,y,c->remote_patch_radius))continue;
        if(quantity>UINT32_MAX)quantity=UINT32_MAX;
        if(place_patch(world,x,y,c->remote_patch_radius,type,(uint32_t)quantity,
            CHANNEL_PATCH|UINT64_C(0x100)+(uint64_t)placed)>=3U)++placed;
    }
}

FactoryResult factory_world_generate(FactoryWorld *world,
    const FactoryWorldGenerationConfig *config)
{
    FactoryWorld temporary;
    size_t count;
    int32_t sx,sy;
    if(world==NULL||!config_valid(config))return FACTORY_RESULT_INVALID_ARGUMENT;
    if(world->sealed)return FACTORY_RESULT_INVALID_STATE;
    if(world->width>INT32_MAX||world->height>INT32_MAX)
        return FACTORY_RESULT_WORLD_GENERATION_FAILED;
    if((size_t)world->width>SIZE_MAX/(size_t)world->height)
        return FACTORY_RESULT_WORLD_GENERATION_FAILED;
    count=(size_t)world->width*world->height;
    if(count>SIZE_MAX/sizeof(FactoryTile))return FACTORY_RESULT_OUT_OF_MEMORY;
    temporary=*world;
    temporary.tiles=calloc(count,sizeof(*temporary.tiles));
    if(temporary.tiles==NULL)return FACTORY_RESULT_OUT_OF_MEMORY;
    for(uint32_t y=0U;y<world->height;++y)for(uint32_t x=0U;x<world->width;++x){
        FactoryTile *tile=&temporary.tiles[tile_index(&temporary,x,y)];
        uint16_t water=field_value(world->seed,x,y,config->terrain_scale,CHANNEL_WATER);
        uint16_t rock=field_value(world->seed,x,y,config->terrain_scale,CHANNEL_ROCK);
        tile->terrain=water<config->water_threshold?FACTORY_TERRAIN_WATER:
            (rock<config->rock_threshold?FACTORY_TERRAIN_ROCK:FACTORY_TERRAIN_GROUND);
    }
    sx=factory_world_get_start_x(&temporary);sy=factory_world_get_start_y(&temporary);
    for(int32_t dy=-(int32_t)config->starting_area_radius;
        dy<=(int32_t)config->starting_area_radius;++dy)
        for(int32_t dx=-(int32_t)config->starting_area_radius;
            dx<=(int32_t)config->starting_area_radius;++dx)
            if(dx*dx+dy*dy<=(int32_t)(config->starting_area_radius*config->starting_area_radius)
                &&factory_world_is_in_bounds(&temporary,sx+dx,sy+dy))
                temporary.tiles[tile_index(&temporary,(uint32_t)(sx+dx),
                    (uint32_t)(sy+dy))].terrain=FACTORY_TERRAIN_GROUND;
    if(!place_starters(&temporary,config)){free(temporary.tiles);return FACTORY_RESULT_WORLD_GENERATION_FAILED;}
    place_remote_patches(&temporary,config);
    if(!factory_world_validate(&temporary)){free(temporary.tiles);return FACTORY_RESULT_WORLD_GENERATION_FAILED;}
    free(world->tiles);world->tiles=temporary.tiles;
    return FACTORY_RESULT_OK;
}

static void checksum_u32(uint64_t *hash,uint32_t value)
{for(unsigned i=0U;i<4U;++i){*hash^=(uint8_t)(value>>(i*8U));*hash*=UINT64_C(1099511628211);}}
uint64_t factory_world_generation_checksum(const FactoryWorld *world)
{
    uint64_t hash=UINT64_C(1469598103934665603);
    if(!factory_world_validate(world))return 0U;
    checksum_u32(&hash,(uint32_t)world->seed);checksum_u32(&hash,(uint32_t)(world->seed>>32U));
    checksum_u32(&hash,world->width);checksum_u32(&hash,world->height);
    for(size_t i=0U;i<(size_t)world->width*world->height;++i){
        checksum_u32(&hash,(uint32_t)world->tiles[i].terrain);
        checksum_u32(&hash,(uint32_t)world->tiles[i].resource);
        checksum_u32(&hash,world->tiles[i].resource_amount);
        checksum_u32(&hash,world->tiles[i].occupying_entity);
    }
    return hash;
}

#include "AppHdr.h"

#include "tiledgnbuf.h"

#include "env.h"
#include "player.h"
#include "tiledef-dngn.h"
#include "tiledef-icons.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiledoll.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "tilepick-p.h"

DungeonCellBuffer::DungeonCellBuffer(ImageManager *im) :
    m_buf_floor(&im->m_textures[TEX_FLOOR]),
    m_buf_wall(&im->m_textures[TEX_WALL]),
    m_buf_feat(&im->m_textures[TEX_FEAT]),
    m_buf_feat_trans(&im->m_textures[TEX_FEAT], 17),
    m_buf_doll(&im->m_textures[TEX_PLAYER], 17),
    m_buf_main_trans(&im->m_textures[TEX_DEFAULT], 17),
    m_buf_main(&im->m_textures[TEX_DEFAULT]),
    m_buf_spells(&im->m_textures[TEX_GUI]),
    m_buf_skills(&im->m_textures[TEX_GUI]),
    m_buf_commands(&im->m_textures[TEX_GUI]),
    m_buf_icons(&im->m_textures[TEX_ICONS])
{
}

static bool _in_water(const packed_cell &cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

void DungeonCellBuffer::add(const packed_cell &cell, int x, int y)
{
    pack_background(x, y, cell);

    const tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (fg_idx >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
            pack_mcache(entry, x, y, in_water);
        else
            m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y, 0, in_water, false);
    }
    else if (fg_idx == TILEP_PLAYER)
        pack_player(x, y, in_water);
    else if (fg_idx >= TILE_MAIN_MAX)
        m_buf_doll.add(fg_idx, x, y, TILEP_PART_MAX, in_water, false);

    pack_foreground(x, y, cell);
}

void DungeonCellBuffer::add_dngn_tile(int tileidx, int x, int y,
                                      bool in_water)
{
    ASSERT(tileidx < TILE_FEAT_MAX);

    if (tileidx < TILE_FLOOR_MAX)
        m_buf_floor.add(tileidx, x, y);
    else if (tileidx < TILE_WALL_MAX)
        m_buf_wall.add(tileidx, x, y);
    else if (in_water)
        m_buf_feat_trans.add(tileidx, x, y, 0, true, false);
    else
        m_buf_feat.add(tileidx, x, y);
}

void DungeonCellBuffer::add_main_tile(int tileidx, int x, int y)
{
    tileidx_t base = tileidx_known_base_item(tileidx);
    if (base)
        m_buf_main.add(base, x, y);

    m_buf_main.add(tileidx, x, y);
}

void DungeonCellBuffer::add_main_tile(int tileidx, int x, int y, int ox, int oy)
{
    tileidx_t base = tileidx_known_base_item(tileidx);
    if (base)
        m_buf_main.add(base, x, y, ox, oy, false);

    m_buf_main.add(tileidx, x, y, ox, oy, false);
}

void DungeonCellBuffer::add_spell_tile(int tileidx, int x, int y)
{
    m_buf_spells.add(tileidx, x, y);
}

void DungeonCellBuffer::add_skill_tile(int tileidx, int x, int y)
{
    m_buf_skills.add(tileidx, x, y);
}

void DungeonCellBuffer::add_command_tile(int tileidx, int x, int y)
{
    m_buf_commands.add(tileidx, x, y);
}

void DungeonCellBuffer::add_icons_tile(int tileidx, int x, int y)
{
    m_buf_icons.add(tileidx, x, y);
}

void DungeonCellBuffer::add_icons_tile(int tileidx, int x, int y,
                                       int ox, int oy)
{
    m_buf_icons.add(tileidx, x, y, ox, oy, false);
}

void DungeonCellBuffer::clear()
{
    m_buf_floor.clear();
    m_buf_wall.clear();
    m_buf_feat.clear();
    m_buf_feat_trans.clear();
    m_buf_doll.clear();
    m_buf_main_trans.clear();
    m_buf_main.clear();
    m_buf_spells.clear();
    m_buf_skills.clear();
    m_buf_commands.clear();
    m_buf_icons.clear();
}

void DungeonCellBuffer::draw()
{
    m_buf_floor.draw();
    m_buf_wall.draw();
    m_buf_feat.draw();
    m_buf_feat_trans.draw();
    m_buf_doll.draw();
    m_buf_main_trans.draw();
    m_buf_main.draw();
    m_buf_skills.draw();
    m_buf_spells.draw();
    m_buf_commands.draw();
    m_buf_icons.draw();
}

void DungeonCellBuffer::add_blood_overlay(int x, int y, const packed_cell &cell,
                                          bool is_wall)
{
    if (cell.is_liquefied && !is_wall)
    {
        int offset = cell.flv.special % tile_dngn_count(TILE_LIQUEFACTION);
        m_buf_feat.add(TILE_LIQUEFACTION + offset, x, y);
    }
    else if (cell.is_bloody)
    {
        tileidx_t basetile;
        if (is_wall)
        {
            basetile = cell.old_blood ? TILE_WALL_OLD_BLOOD : TILE_WALL_BLOOD_S;
            basetile += tile_dngn_count(basetile) * cell.blood_rotation;
        }
        else
            basetile = TILE_BLOOD;
        const int offset = cell.flv.special % tile_dngn_count(basetile);
        m_buf_feat.add(basetile + offset, x, y);
    }
    else if (cell.is_moldy)
    {
        int offset = cell.flv.special % tile_dngn_count(TILE_MOLD);
        m_buf_feat.add(TILE_MOLD + offset, x, y);
    }
    else if (cell.glowing_mold)
    {
        int offset = cell.flv.special % tile_dngn_count(TILE_GLOWING_MOLD);
        m_buf_feat.add(TILE_GLOWING_MOLD + offset, x, y);
    }
}

void DungeonCellBuffer::pack_background(int x, int y, const packed_cell &cell)
{
    const tileidx_t bg = cell.bg;
    const tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;

    if (cell.mangrove_water && bg_idx > TILE_DNGN_UNSEEN)
        m_buf_feat.add(TILE_DNGN_SHALLOW_WATER, x, y);

    if (bg_idx >= TILE_DNGN_FIRST_TRANSPARENT)
        add_dngn_tile(cell.flv.floor, x, y);

    // Draw blood beneath feature tiles.
    if (bg_idx > TILE_WALL_MAX)
        add_blood_overlay(x, y, cell);

    add_dngn_tile(bg_idx, x, y, cell.mangrove_water);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        // Draw blood on top of wall tiles.
        if (bg_idx <= TILE_WALL_MAX)
            add_blood_overlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

        for (int i = 0; i < cell.num_dngn_overlay; ++i)
            add_dngn_tile(cell.dngn_overlay[i], x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (bg & TILE_FLAG_KRAKEN_NW)
                m_buf_feat.add(TILE_KRAKEN_OVERLAY_NW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NW)
                m_buf_feat.add(TILE_ELDRITCH_OVERLAY_NW, x, y);
            if (bg & TILE_FLAG_KRAKEN_NE)
                m_buf_feat.add(TILE_KRAKEN_OVERLAY_NE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NE)
                m_buf_feat.add(TILE_ELDRITCH_OVERLAY_NE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SE)
                m_buf_feat.add(TILE_KRAKEN_OVERLAY_SE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SE)
                m_buf_feat.add(TILE_ELDRITCH_OVERLAY_SE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SW)
                m_buf_feat.add(TILE_KRAKEN_OVERLAY_SW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SW)
                m_buf_feat.add(TILE_ELDRITCH_OVERLAY_SW, x, y);
        }

        if (cell.halo == HALO_MONSTER)
            m_buf_feat.add(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (cell.is_sanctuary)
                m_buf_feat.add(TILE_SANCTUARY, x, y);
            if (cell.is_silenced)
                m_buf_feat.add(TILE_SILENCED, x, y);
            if (cell.is_suppressed)
                m_buf_feat.add(TILE_SUPPRESSED, x, y);
            if (cell.halo == HALO_RANGE)
                m_buf_feat.add(TILE_HALO_RANGE, x, y);
            if (cell.halo == HALO_UMBRA)
                m_buf_feat.add(TILE_UMBRA + random2(4), x, y);

            if (cell.orb_glow)
                m_buf_feat.add(TILE_ORB_GLOW + cell.orb_glow - 1, x, y);
            if (cell.quad_glow)
                m_buf_feat.add(TILE_QUAD_GLOW, x, y);
            if (cell.disjunct)
                m_buf_feat.add(TILE_DISJUNCT + cell.disjunct - 1, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                m_buf_feat.add(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                m_buf_feat.add(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

        if (bg & TILE_FLAG_RAY)
            m_buf_feat.add(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            m_buf_feat.add(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

void DungeonCellBuffer::pack_foreground(int x, int y, const packed_cell &cell)
{
    const tileidx_t fg = cell.fg;
    const tileidx_t bg = cell.bg;
    const tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        const tileidx_t base_idx = tileidx_known_base_item(fg_idx);

        if (in_water)
        {
            if (base_idx)
                m_buf_main_trans.add(base_idx, x, y, 0, true, false);
            m_buf_main_trans.add(fg_idx, x, y, 0, true, false);
        }
        else
        {
            if (base_idx)
                m_buf_main.add(base_idx, x, y);

            m_buf_main.add(fg_idx, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        m_buf_icons.add(TILEI_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        m_buf_icons.add(TILEI_SOMETHING_UNDER, x, y);

    int status_shift = 0;
    switch (fg & TILE_FLAG_MIMIC_MASK)
    {
    case TILE_FLAG_MIMIC_INEPT:
        m_buf_icons.add(TILEI_INEPT_MIMIC, x, y);
        break;
    case TILE_FLAG_MIMIC:
        m_buf_icons.add(TILEI_MIMIC, x, y);
        break;
    case TILE_FLAG_MIMIC_RAVEN:
        m_buf_icons.add(TILEI_RAVENOUS_MIMIC, x, y);
        break;
    default: ;
    }

    //The berserk icon is in the lower right, so status_shift doesn't need changing.
    if (fg & TILE_FLAG_BERSERK)
        m_buf_icons.add(TILEI_BERSERK, x, y);

    // Pet mark
    if (fg & TILE_FLAG_ATT_MASK)
    {
        const tileidx_t att_flag = fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_PET)
        {
            m_buf_icons.add(TILEI_HEART, x, y);
            status_shift += 10;
        }
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
        {
            m_buf_icons.add(TILEI_GOOD_NEUTRAL, x, y);
            status_shift += 7;
        }
        else if (att_flag == TILE_FLAG_NEUTRAL)
        {
            m_buf_icons.add(TILEI_NEUTRAL, x, y);
            status_shift += 7;
        }
    }

    if (fg & TILE_FLAG_BEH_MASK)
    {
        const tileidx_t beh_flag = fg & TILE_FLAG_BEH_MASK;
        if (beh_flag == TILE_FLAG_STAB)
        {
            m_buf_icons.add(TILEI_STAB_BRAND, x, y);
            status_shift += 12;
        }
        else if (beh_flag == TILE_FLAG_MAY_STAB)
        {
            m_buf_icons.add(TILEI_MAY_STAB_BRAND, x, y);
            status_shift += 7;
        }
        else if (beh_flag == TILE_FLAG_FLEEING)
        {
            m_buf_icons.add(TILEI_FLEEING, x, y);
            status_shift += 3;
        }
    }

    if (fg & TILE_FLAG_POISON)
    {
        m_buf_icons.add(TILEI_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_STICKY_FLAME)
    {
        m_buf_icons.add(TILEI_STICKY_FLAME, x, y, -status_shift, 0);
        status_shift += 7;
    }
    if (fg & TILE_FLAG_INNER_FLAME)
    {
        m_buf_icons.add(TILEI_INNER_FLAME, x, y, -status_shift, 0);
        status_shift += 7;
    }
    if (fg & TILE_FLAG_CONSTRICTED)
    {
        m_buf_icons.add(TILEI_CONSTRICTED, x, y, -status_shift, 0);
        status_shift += 11;
    }
    if (fg & TILE_FLAG_GLOWING)
    {
        //if (!cell.halo)
        //    m_buf_feat.add(TILE_HALO, x, y);

        m_buf_icons.add(TILEI_GLOWING, x, y, -status_shift, 0);
        status_shift += 7;
    }
    if (fg & TILE_FLAG_HASTED)
    {
        m_buf_icons.add(TILEI_HASTED, x, y, -status_shift, 0);
        status_shift += 6;
    }
    if (fg & TILE_FLAG_SLOWED)
    {
        m_buf_icons.add(TILEI_SLOWED, x, y, -status_shift, 0);
        status_shift += 6;
    }
    if (fg & TILE_FLAG_MIGHT)
    {
        m_buf_icons.add(TILEI_MIGHT, x, y, -status_shift, 0);
        status_shift += 6;
    }
    if (fg & TILE_FLAG_PAIN_MIRROR)
    {
        m_buf_icons.add(TILEI_PAIN_MIRROR, x, y, -status_shift, 0);
        status_shift += 7;
    }
    if (fg & TILE_FLAG_PETRIFYING)
    {
        m_buf_icons.add(TILEI_PETRIFYING, x, y, -status_shift, 0);
        status_shift += 6;
    }
    if (fg & TILE_FLAG_PETRIFIED)
    {
        m_buf_icons.add(TILEI_PETRIFIED, x, y, -status_shift, 0);
        status_shift += 6;
    }
    if (fg & TILE_FLAG_BLIND)
    {
        m_buf_icons.add(TILEI_BLIND, x, y, -status_shift, 0);
        status_shift += 10;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        m_buf_icons.add(TILEI_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        m_buf_icons.add(TILEI_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        m_buf_icons.add(TILEI_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        m_buf_icons.add(TILEI_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        m_buf_icons.add(TILEI_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        m_buf_icons.add(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        m_buf_icons.add(TILEI_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
        m_buf_icons.add(TILEI_TUTORIAL_CURSOR, x, y);
    else if (bg & TILE_FLAG_CURSOR)
    {
        int type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILEI_CURSOR : TILEI_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILEI_CURSOR3;

        m_buf_icons.add(type, x, y);
    }

    if (cell.travel_trail & 0xF)
    {
        m_buf_icons.add(TILEI_TRAVEL_PATH_FROM +
                        (cell.travel_trail & 0xF) - 1, x, y);
    }
    if (cell.travel_trail & 0xF0)
    {
        m_buf_icons.add(TILEI_TRAVEL_PATH_TO +
                        ((cell.travel_trail & 0xF0) >> 4) - 1, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        tileidx_t mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            m_buf_icons.add(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            m_buf_icons.add(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            m_buf_icons.add(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            m_buf_icons.add(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            m_buf_icons.add(TILEI_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        tileidx_t demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            m_buf_icons.add(TILEI_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            m_buf_icons.add(TILEI_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            m_buf_icons.add(TILEI_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            m_buf_icons.add(TILEI_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            m_buf_icons.add(TILEI_DEMON_NUM5, x, y);
    }
}

void DungeonCellBuffer::pack_player(int x, int y, bool submerged)
{
    dolls_data result = player_doll;
    fill_doll_equipment(result);
    pack_doll(result, x, y, submerged, false);
}

void DungeonCellBuffer::pack_doll(const dolls_data &doll, int x, int y,
                                  bool submerged, bool ghost)
{
    pack_doll_buf(m_buf_doll, doll, x, y, submerged, ghost);
}


void DungeonCellBuffer::pack_mcache(mcache_entry *entry, int x, int y,
                                    bool submerged)
{
    ASSERT(entry);

    bool trans = entry->transparent();

    const dolls_data *doll = entry->doll();
    if (doll)
        pack_doll(*doll, x, y, submerged, trans);

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        m_buf_doll.add(dinfo[i].idx, x, y, TILEP_PART_MAX, submerged, trans,
                       dinfo[i].ofs_x, dinfo[i].ofs_y);
    }
}

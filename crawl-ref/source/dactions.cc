/**
 * @file
 * @brief Delayed level actions.
**/

#include "AppHdr.h"

#include "dactions.h"

#include "coordit.h"
#include "debug.h"
#include "decks.h"
#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "travel.h"
#include "view.h"

#ifdef DEBUG_DIAGNOSTICS
static const char *daction_names[] =
{
    "holy beings go hostile",
    "unholy/evil go hostile",
    "unclean/chaotic go hostile",
    "spellcasters go hostile",
    "yred slaves go hostile",
    "beogh orcs and their summons go hostile",
    "fellow slimes go hostile",
    "plants go hostile (allowing reconversion)",
    0, 0, 0, 0, 0, 0, 0, 0,

    // Actions not needing a counter.
    "old enslaved souls go poof",
    "holy beings allow another conversion attempt",
#if TAG_MAJOR_VERSION > 34
    "slimes allow another conversion attempt",
#endif
    "holy beings go neutral",
    "Trog's gifts go hostile",
    "shuffle decks",
    "reapply passive mapping",
    "remove Jiyva altars",
    "Pikel's slaves go good-neutral",
    "corpses rot",
    "Tomb loses -cTele",
#if TAG_MAJOR_VERSION == 34
    "slimes allow another conversion attempt",
#endif
};
#endif

bool mons_matches_daction(const monster* mon, daction_type act)
{
    if (!mon || !mon->alive())
        return false;

    switch (act)
    {
    case DACT_ALLY_HOLY:
        return (mon->wont_attack() && is_good_god(mon->god));
    case DACT_ALLY_UNHOLY_EVIL:
        return (mon->wont_attack() && (mon->is_unholy() || mon->is_evil()));
    case DACT_ALLY_UNCLEAN_CHAOTIC:
        return (mon->wont_attack() && (mon->is_unclean() || mon->is_chaotic()));
    case DACT_ALLY_SPELLCASTER:
        return (mon->wont_attack() && mon->is_actual_spellcaster());
    case DACT_ALLY_YRED_SLAVE:
        // Changed: we don't force enslavement of those merely marked.
        return is_yred_undead_slave(mon);
    case DACT_ALLY_BEOGH: // both orcs and demons summoned by high priests
        return (mon->wont_attack() && mons_is_god_gift(mon, GOD_BEOGH));
    case DACT_ALLY_SLIME:
        return is_fellow_slime(mon);
    case DACT_ALLY_PLANT:
        // No check for friendliness since we pretend all plants became friendly
        // the moment you converted to Fedhas.
        return mons_is_plant(mon);

    // Not a stored counter:
    case DACT_ALLY_TROG:
        return (mon->friendly() && mons_is_god_gift(mon, GOD_TROG));
    case DACT_HOLY_PETS_GO_NEUTRAL:
        return (mon->friendly()
                && !mon->has_ench(ENCH_CHARM)
                && mon->is_holy()
                && mons_is_god_gift(mon, GOD_SHINING_ONE));
    case DACT_PIKEL_SLAVES:
        return (mon->type == MONS_SLAVE
                && testbits(mon->flags, MF_BAND_MEMBER)
                && mon->props.exists("pikel_band")
                && mon->mname != "freed slave");

    case DACT_OLD_ENSLAVED_SOULS_POOF:
        return mons_enslaved_soul(mon);

    case DACT_HOLY_NEW_ATTEMPT:
        return mon->is_holy();

    case DACT_SLIME_NEW_ATTEMPT:
        return mons_is_slime(mon);

    default:
        return false;
    }
}

void update_da_counters(LevelInfo *lev)
{
    for (int act = 0; act < NUM_DA_COUNTERS; act++)
        lev->da_counters[act] = 0;

    for (monster_iterator mi; mi; ++mi)
        for (int act = 0; act < NUM_DA_COUNTERS; act++)
            if (mons_matches_daction(*mi, static_cast<daction_type>(act)))
                lev->da_counters[act]++;
}

void add_daction(daction_type act)
{
#ifdef DEBUG_DIAGNOSTICS
    COMPILE_CHECK(ARRAYSZ(daction_names) == NUM_DACTIONS);
#endif

    dprf("scheduling delayed action: %s", daction_names[act]);
    you.dactions.push_back(act);

    // If we're removing a counted monster type, zero the counter even though
    // it hasn't been actually removed from the levels yet.
    if (act < NUM_DA_COUNTERS)
        travel_cache.clear_da_counter(act);

    // Immediately apply it to the current level.
    catchup_dactions();

    // And now to any monsters in transit.
    apply_daction_to_transit(act);

}

void apply_daction_to_mons(monster* mon, daction_type act, bool local)
{
    switch (act)
    {
        case DACT_ALLY_HOLY:
        case DACT_ALLY_UNHOLY_EVIL:
        case DACT_ALLY_UNCLEAN_CHAOTIC:
        case DACT_ALLY_SPELLCASTER:
        case DACT_ALLY_YRED_SLAVE:
        case DACT_ALLY_BEOGH:
        case DACT_ALLY_SLIME:
        case DACT_ALLY_PLANT:
        case DACT_ALLY_TROG:
            dprf("going hostile: %s", mon->name(DESC_PLAIN, true).c_str());
            mon->attitude = ATT_HOSTILE;
            mon->del_ench(ENCH_CHARM, true);
            if (local)
                behaviour_event(mon, ME_ALERT, &you);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.
            mons_att_changed(mon);

            // If you reconvert to Fedhas/Jiyva, plants/slimes will
            // love you again.
            if (act == DACT_ALLY_PLANT || act == DACT_ALLY_SLIME)
                mon->flags &= ~MF_ATT_CHANGE_ATTEMPT;

            // No global message for Trog.
            if (act == DACT_ALLY_TROG && local)
                simple_monster_message(mon, " turns against you!");
            break;

        case DACT_OLD_ENSLAVED_SOULS_POOF:
            simple_monster_message(mon, " is freed.");
            // The monster disappears.
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
            break;

        case DACT_HOLY_NEW_ATTEMPT:
        case DACT_SLIME_NEW_ATTEMPT:
            mon->flags &= ~MF_ATT_CHANGE_ATTEMPT;
            break;

        case DACT_HOLY_PETS_GO_NEUTRAL:
        case DACT_PIKEL_SLAVES:
            // monster changes attitude
            mon->attitude = ATT_GOOD_NEUTRAL;
            mons_att_changed(mon);

            if (act == DACT_PIKEL_SLAVES)
            {
                mon->flags |= MF_NAME_REPLACE | MF_NAME_DESCRIPTOR
                                  | MF_NAME_NOCORPSE;
                mon->mname = "freed slave";
            }
            else if (local)
                simple_monster_message(mon, " becomes indifferent.");
            mon->behaviour = BEH_WANDER;
            break;

        // The other dactions do not affect monsters directly.
        default:
            break;
    }
}

static void _apply_daction(daction_type act)
{
    ASSERT(act >= 0 && act < NUM_DACTIONS);
    dprf("applying delayed action: %s", daction_names[act]);

    switch (act)
    {
    case DACT_ALLY_HOLY:
    case DACT_ALLY_UNHOLY_EVIL:
    case DACT_ALLY_UNCLEAN_CHAOTIC:
    case DACT_ALLY_SPELLCASTER:
    case DACT_ALLY_YRED_SLAVE:
    case DACT_ALLY_BEOGH:
    case DACT_ALLY_SLIME:
    case DACT_ALLY_PLANT:
    case DACT_ALLY_TROG:
    case DACT_OLD_ENSLAVED_SOULS_POOF:
    case DACT_HOLY_NEW_ATTEMPT:
    case DACT_SLIME_NEW_ATTEMPT:
    case DACT_HOLY_PETS_GO_NEUTRAL:
    case DACT_PIKEL_SLAVES:
        for (monster_iterator mi; mi; ++mi)
        {
            if (mons_matches_daction(*mi, act))
                apply_daction_to_mons(*mi, act, true);
        }
        break;

    case DACT_SHUFFLE_DECKS:
        shuffle_all_decks_on_level();
        break;
    case DACT_REAUTOMAP:
        reautomap_level();
        break;
    case DACT_REMOVE_JIYVA_ALTARS:
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) == DNGN_ALTAR_JIYVA)
                grd(*ri) = DNGN_FLOOR;
        }
        break;
    case DACT_ROT_CORPSES:
        for (int i = 0; i < MAX_ITEMS; i++)
            if (mitm[i].base_type == OBJ_CORPSES && mitm[i].sub_type == CORPSE_BODY)
                mitm[i].special = 1; // thoroughly rotten
        break;
    case DACT_TOMB_CTELE:
        if (player_in_branch(BRANCH_TOMB))
            unset_level_flags(LFLAG_NO_TELE_CONTROL, you.depth != 3);
        break;
    case NUM_DA_COUNTERS:
    case NUM_DACTIONS:
        ;
    }
}

void catchup_dactions()
{
    while (env.dactions_done < you.dactions.size())
        _apply_daction(you.dactions[env.dactions_done++]);
}

unsigned int query_da_counter(daction_type c)
{
    return travel_cache.query_da_counter(c) + count_daction_in_transit(c);
}

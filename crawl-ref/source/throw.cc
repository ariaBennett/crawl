/**
 * @file
 * @brief Throwing and launching stuff.
**/

#include "AppHdr.h"

#include "throw.h"

#include <math.h>

#include "externs.h"

#include "cloud.h"
#include "colour.h"
#include "command.h"
#include "delay.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "fineff.h"
#include "godconduct.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "options.h"
#include "shout.h"
#include "skills2.h"
#include "stuff.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"
#include "viewchar.h"

static int  _fire_prompt_for_item();
static bool _fire_validate_item(int selected, std::string& err);

bool item_is_quivered(const item_def &item)
{
    return (item.link == you.m_quiver->get_fire_item());
}

int get_next_fire_item(int current, int direction)
{
    std::vector<int> fire_order;
    you.m_quiver->get_fire_order(fire_order, true);

    if (fire_order.empty())
        return -1;

    int next = direction > 0 ? 0 : -1;
    for (unsigned i = 0; i < fire_order.size(); i++)
    {
        if (fire_order[i] == current)
        {
            next = i + direction;
            break;
        }
    }

    next = (next + fire_order.size()) % fire_order.size();
    return fire_order[next];
}

class fire_target_behaviour : public targetting_behaviour
{
public:
    fire_target_behaviour()
        : chosen_ammo(false),
          selected_from_inventory(false),
          need_redraw(false)
    {
        m_slot = you.m_quiver->get_fire_item(&m_noitem_reason);
        set_prompt();
    }

    // targetting_behaviour API
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw() const { return need_redraw; }
    virtual void clear_redraw()        { need_redraw = false; }
    virtual void update_top_prompt(std::string* p_top_prompt);
    virtual std::vector<std::string> get_monster_desc(const monster_info& mi);

public:
    const item_def* active_item() const;
    // FIXME: these should be privatized and given accessors.
    int m_slot;
    bool chosen_ammo;

private:
    void set_prompt();
    void cycle_fire_item(bool forward);
    void pick_fire_item_from_inventory();
    void display_help();

    std::string prompt;
    std::string m_noitem_reason;
    std::string internal_prompt;
    bool selected_from_inventory;
    bool need_redraw;
};

void fire_target_behaviour::update_top_prompt(std::string* p_top_prompt)
{
    *p_top_prompt = internal_prompt;
}

const item_def* fire_target_behaviour::active_item() const
{
    if (m_slot == -1)
        return NULL;
    else
        return &you.inv[m_slot];
}

void fire_target_behaviour::set_prompt()
{
    std::string old_prompt = internal_prompt; // Keep for comparison at the end.
    internal_prompt.clear();

    // Figure out if we have anything else to cycle to.
    const int next_item = get_next_fire_item(m_slot, +1);
    const bool no_other_items = (next_item == -1 || next_item == m_slot);

    std::ostringstream msg;

    // Build the action.
    if (!active_item())
        msg << "Firing ";
    else
    {
        const launch_retval projected = is_launched(&you, you.weapon(),
                                                    *active_item());
        switch (projected)
        {
        case LRET_FUMBLED:  msg << "Awkwardly throwing "; break;
        case LRET_LAUNCHED: msg << "Firing ";             break;
        case LRET_THROWN:   msg << "Throwing ";           break;
        }
    }

    // And a key hint.
    msg << (no_other_items ? "(i - inventory)"
                           : "(i - inventory. (,) - cycle)")
        << ": ";

    // Describe the selected item for firing.
    if (!active_item())
        msg << "<red>" << m_noitem_reason << "</red>";
    else
    {
        const char* colour = (selected_from_inventory ? "lightgrey" : "w");
        msg << "<" << colour << ">"
            << active_item()->name(DESC_INVENTORY_EQUIP)
            << "</" << colour << ">";
    }

    // Write it out.
    internal_prompt += msg.str();

    // Never unset need_redraw here, because we might have cleared the
    // screen or something else which demands a redraw.
    if (internal_prompt != old_prompt)
        need_redraw = true;
}

// Cycle to the next (forward == true) or previous (forward == false)
// fire item.
void fire_target_behaviour::cycle_fire_item(bool forward)
{
    const int next = get_next_fire_item(m_slot, forward ? 1 : -1);
    if (next != m_slot && next != -1)
    {
        m_slot = next;
        selected_from_inventory = false;
        chosen_ammo = true;
    }
    set_prompt();
}

void fire_target_behaviour::pick_fire_item_from_inventory()
{
    need_redraw = true;
    std::string err;
    const int selected = _fire_prompt_for_item();
    if (selected >= 0 && _fire_validate_item(selected, err))
    {
        m_slot = selected;
        selected_from_inventory = true;
        chosen_ammo = true;
    }
    else if (!err.empty())
    {
        mpr(err);
        more();
    }
    set_prompt();
}

void fire_target_behaviour::display_help()
{
    show_targetting_help();
    redraw_screen();
    need_redraw = true;
    set_prompt();
}

command_type fire_target_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    switch (key)
    {
    case '(': case CONTROL('N'): cycle_fire_item(true);  return (CMD_NO_CMD);
    case ')': case CONTROL('P'): cycle_fire_item(false); return (CMD_NO_CMD);
    case 'i': pick_fire_item_from_inventory(); return (CMD_NO_CMD);
    case '?': display_help(); return (CMD_NO_CMD);
    case CMD_TARGET_CANCEL: chosen_ammo = false; break;
    }

    return targetting_behaviour::get_command(key);
}

std::vector<std::string> fire_target_behaviour::get_monster_desc(const monster_info& mi)
{
    std::vector<std::string> descs;
    if (const item_def* item = active_item())
    {
        if (get_ammo_brand(*item) == SPMSL_SILVER && mi.is(MB_CHAOTIC))
            descs.push_back("chaotic");
    }
    return descs;
}

static bool _fire_choose_item_and_target(int& slot, dist& target,
                                         bool teleport = false)
{
    fire_target_behaviour beh;
    const bool was_chosen = (slot != -1);

    if (was_chosen)
    {
        std::string warn;
        if (!_fire_validate_item(slot, warn))
        {
            mpr(warn.c_str());
            return (false);
        }
        // Force item to be the prechosen one.
        beh.m_slot = slot;
    }

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.needs_path = !teleport;
    args.behaviour = &beh;

    direction(target, args);

    if (!beh.active_item())
    {
        canned_msg(MSG_OK);
        return (false);
    }
    if (!target.isValid)
    {
        if (target.isCancel)
            canned_msg(MSG_OK);
        return (false);
    }

    you.m_quiver->on_item_fired(*beh.active_item(), beh.chosen_ammo);
    you.redraw_quiver = true;
    slot = beh.m_slot;

    return (true);
}

// Bring up an inventory screen and have user choose an item.
// Returns an item slot, or -1 on abort/failure
// On failure, returns error text, if any.
static int _fire_prompt_for_item()
{
    if (inv_count() < 1)
        return -1;

    int slot = prompt_invent_item("Fire/throw which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_THROWABLE, true, true, true, 0, -1,
                                   NULL, OPER_FIRE);

    if (slot == PROMPT_ABORT || slot == PROMPT_NOTHING)
        return -1;

    return slot;
}

// Returns false and err text if this item can't be fired.
static bool _fire_validate_item(int slot, std::string &err)
{
    if (slot == you.equip[EQ_WEAPON]
        && (you.inv[slot].base_type == OBJ_WEAPONS
            || you.inv[slot].base_type == OBJ_STAVES)
        && you.inv[slot].cursed())
    {
        err = "That weapon is stuck to your " + you.hand_name(false) + "!";
        return (false);
    }
    else if (item_is_worn(slot))
    {
        err = "You are wearing that object!";
        return (false);
    }
    else if (you.inv[slot].base_type == OBJ_ORBS)
    {
       err = "You don't feel like leaving the orb behind!";
       return (false);
    }
    return (true);
}

// Returns true if warning is given.
bool fire_warn_if_impossible(bool silent)
{
    if (you.species == SP_FELID)
    {
        if (!silent)
            mpr("You can't grasp things well enough to throw them.");
        return (true);
    }

    // If you can't wield it, you can't throw it.
    if (!form_can_wield())
    {
        if (!silent)
            canned_msg(MSG_PRESENT_FORM);
        return (true);
    }

    if (you.attribute[ATTR_HELD])
    {
        const item_def *weapon = you.weapon();
        if (!weapon || !is_range_weapon(*weapon))
        {
            if (!silent)
                mprf("You cannot throw anything while %s.", held_status());
            return (true);
        }
        else if (weapon->sub_type != WPN_BLOWGUN)
        {
            if (!silent)
                mprf("You cannot shoot with your %s while %s.",
                     weapon->name(DESC_BASENAME).c_str(), held_status());
            return (true);
        }
        // Else shooting is possible.
    }
    if (you.berserk())
    {
        if (!silent)
            canned_msg(MSG_TOO_BERSERK);
        return (true);
    }
    return (false);
}

static bool _autoswitch_to_ranged()
{
    if (you.equip[EQ_WEAPON] != 0 && you.equip[EQ_WEAPON] != 1)
        return false;

    int item_slot = you.equip[EQ_WEAPON] ^ 1;
    const item_def& launcher = you.inv[item_slot];
    if (!is_range_weapon(launcher))
        return false;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
       if (iter->launched_by(launcher))
       {
          if (!wield_weapon(true, item_slot))
              return false;

          you.turn_is_over = true;
          //XXX Hacky. Should use a delay instead.
          macro_buf_add(command_to_key(CMD_FIRE));
          return true;
       }

    return false;
}

int get_ammo_to_shoot(int item, dist &target, bool teleport)
{
    if (fire_warn_if_impossible())
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return (-1);
    }

    if (Options.auto_switch && you.m_quiver->get_fire_item() == -1
       && _autoswitch_to_ranged())
    {
        return (-1);
    }

    if (!_fire_choose_item_and_target(item, target, teleport))
        return (-1);

    std::string warn;
    if (!_fire_validate_item(item, warn))
    {
        mpr(warn.c_str());
        return (-1);
    }
    return (item);
}

// If item == -1, prompt the user.
// If item passed, it will be put into the quiver.
void fire_thing(int item)
{
    dist target;
    item = get_ammo_to_shoot(item, target);
    if (item == -1)
        return;

    if (check_warning_inscriptions(you.inv[item], OPER_FIRE))
    {
        bolt beam;
        throw_it(beam, item, false, 0, &target);
    }
}

// Basically does what throwing used to do: throw an item without changing
// the quiver.
void throw_item_no_quiver()
{
    if (fire_warn_if_impossible())
    {
        flush_input_buffer(FLUSH_ON_FAILURE);
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    std::string warn;
    int slot = _fire_prompt_for_item();

    if (slot == -1)
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!_fire_validate_item(slot, warn))
    {
        mpr(warn.c_str());
        return;
    }

    // Okay, item is valid.
    bolt beam;
    throw_it(beam, slot);
}

// Returns delay multiplier numerator (denominator should be 100) for the
// launcher with the currently equipped shield.
static int _launcher_shield_slowdown(const item_def &launcher,
                                     const item_def *shield)
{
    int speed_adjust = 100;
    if (!shield)
        return (speed_adjust);

    const int shield_type = shield->sub_type;
    hands_reqd_type hands = hands_reqd(launcher, you.body_size());

    switch (hands)
    {
    default:
    case HANDS_ONE:
        break;

    case HANDS_HALF:
        speed_adjust = shield_type == ARM_BUCKLER  ? 105 :
                       shield_type == ARM_SHIELD   ? 125 :
                                                     150;
        break;

    case HANDS_TWO:
        speed_adjust = shield_type == ARM_BUCKLER  ? 125 :
                       shield_type == ARM_SHIELD   ? 150 :
                                                     200;
        break;
    }

    // Adjust for shields skill.
    if (speed_adjust > 100)
        speed_adjust -= you.skill_rdiv(SK_SHIELDS, speed_adjust - 100, 27 * 2);

    return (speed_adjust);
}

// Returns the attack cost of using the launcher, taking skill and shields
// into consideration. NOTE: You must pass in the shield; if you send in
// NULL, this function assumes no shield is in use.
int launcher_final_speed(const item_def &launcher, const item_def *shield, bool scaled)
{
    const int  str_weight   = weapon_str_weight(launcher);
    const int  dex_weight   = 10 - str_weight;
    const skill_type launcher_skill = range_skill(launcher);
    const int shoot_skill4 = you.skill(launcher_skill, 4);
    const int bow_brand = get_weapon_brand(launcher);

    int speed_base = 10 * property(launcher, PWPN_SPEED);
    int speed_min = 70;
    int speed_stat = str_weight * you.strength() + dex_weight * you.dex();

    // Reduce runaway bow overpoweredness.
    if (launcher_skill == SK_BOWS)
        speed_min = 60;

    if (shield)
    {
        const int speed_adjust = _launcher_shield_slowdown(launcher, shield);

        // Shields also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    // Do the same when trying to shoot while held in a net
    // (only possible with blowguns).
    if (you.attribute[ATTR_HELD])
    {
        int speed_adjust = 105; // Analogous to buckler and one-handed weapon.
        speed_adjust -= you.skill_rdiv(SK_THROWING, speed_adjust - 100, 27 * 2);

        // Also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    int speed = speed_base - shoot_skill4 * speed_stat / 250;
    if (speed < speed_min)
        speed = speed_min;

    if (bow_brand == SPWPN_SPEED)
    {
        // Speed nerf as per 4.1. Even with the nerf, bows of speed are the
        // best bows, bar none.
        speed = 2 * speed / 3;
    }

    if (scaled && you.duration[DUR_FINESSE])
    {
        ASSERT(!you.duration[DUR_BERSERK]);
        // Need to undo haste by hand.
        if (you.duration[DUR_HASTE])
            speed = haste_mul(speed);
        speed /= 2;
    }

    return (speed);
}

// Determines if the combined launcher + ammo brands produce a
// fire/frost/chaos beam.
bool elemental_missile_beam(int launcher_brand, int ammo_brand)
{
    if (launcher_brand == SPWPN_FLAME && ammo_brand == SPMSL_FROST ||
        launcher_brand == SPWPN_FROST && ammo_brand == SPMSL_FLAME)
    {
        return (false);
    }
    if (ammo_brand == SPMSL_CHAOS || ammo_brand == SPMSL_FROST || ammo_brand == SPMSL_FLAME)
        return (true);
    if (ammo_brand != SPMSL_NORMAL)
        return (false);
    return (launcher_brand == SPWPN_CHAOS || launcher_brand == SPWPN_FROST ||
            launcher_brand == SPWPN_FLAME);
}

static bool _poison_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (!victim->alive() || victim->res_poison() > 0)
        return (false);

    if (beam.is_tracer)
        return (true);

    int levels = 0;

    actor* agent = beam.agent();

    if (dmg > 0 || beam.ench_power == AUTOMATIC_HIT
                   && x_chance_in_y(90 - 3 * victim->armour_class(), 100))
    {
        levels = 1 + random2(3);
    }

    if (levels <= 0)
        return (false);

    victim->poison(agent, levels);

    return (true);
}

static bool _item_penetrates_victim(const bolt &beam, int &used)
{
    if (beam.aimed_at_feet)
        return (false);

    used = 0;

    return (true);
}

static bool _silver_damages_victim(bolt &beam, actor* victim, int &dmg,
                                   std::string &dmg_msg)
{
    int mutated = 0;

    // For mutation damage, we want to count innate mutations for
    // the demonspawn, but not for other species.
    if (you.species == SP_DEMONSPAWN)
        mutated = how_mutated(true, true);
    else
        mutated = how_mutated(false, true);

    if (victim->is_chaotic()
        || (victim->is_player() && player_is_shapechanged()))
    {
        dmg *= 7;
        dmg /= 4;
    }
    else if (victim->is_player() && mutated > 0)
    {
        int multiplier = 100 + (mutated * 5);

        if (multiplier > 175)
            multiplier = 175;

        dmg = (dmg * multiplier) / 100;
    }
    else
        return (false);

    if (!beam.is_tracer && you.can_see(victim))
       dmg_msg = "The silver sears " + victim->name(DESC_THE) + "!";

    return (false);
}

static bool _dispersal_hit_victim(bolt& beam, actor* victim, int dmg)
{
    const actor* agent = beam.agent();

    if (!victim->alive() || victim == agent || dmg == 0)
        return (false);

    if (beam.is_tracer)
        return (true);

    if (victim->no_tele(true, true))
    {
        if (victim->is_player())
            canned_msg(MSG_STRANGE_STASIS);
        return (false);
    }

    const bool was_seen = you.can_see(victim);
    const bool no_sanct = victim->kill_alignment() == KC_OTHER;

    coord_def pos, pos2;

    int tries = 0;
    do
    {
        if (!random_near_space(victim->pos(), pos, false, true, false,
                               no_sanct))
        {
            return (false);
        }
    }
    while (!victim->is_habitable(pos) && tries++ < 100);

    if (!victim->is_habitable(pos))
        return (false);

    tries = 0;
    do
        random_near_space(victim->pos(), pos2, false, true, false, no_sanct);
    while (!victim->is_habitable(pos2) && tries++ < 100);

    if (!victim->is_habitable(pos2))
        return (false);

    // Pick the square further away from the agent.
    const coord_def from = agent->pos();
    if (in_bounds(pos2)
        && distance(pos2, from) > distance(pos, from))
    {
        pos = pos2;
    }

    if (pos == victim->pos())
        return (false);

    const coord_def oldpos = victim->pos();
    victim->clear_clinging();

    if (victim->is_player())
    {
        stop_delay(true);

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), &you);

        canned_msg(MSG_YOU_BLINK);
        move_player_to_grid(pos, false, true);
    }
    else
    {
        monster* mon = victim->as_monster();

        if (!(mon->flags & MF_WAS_IN_VIEW))
            mon->seen_context = SC_TELEPORT_IN;

        mon->move_to_pos(pos);

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, oldpos, 1 + random2(3), victim);

        mon->apply_location_effects(oldpos);
        mon->check_redraw(oldpos);

        const bool        seen = you.can_see(mon);
        const std::string name = mon->name(DESC_THE);
        if (was_seen && seen)
            mprf("%s blinks!", name.c_str());
        else if (was_seen && !seen)
            mprf("%s vanishes!", name.c_str());
    }

    return (true);
}

static bool _charged_damages_victim(bolt &beam, actor* victim, int &dmg,
                                    std::string &dmg_msg)
{
    if (victim->airborne() || victim->res_elec() > 0 || !one_chance_in(3))
        return (false);

    // A hack and code duplication, but that's easier than adding accounting
    // for each of multiple brands.
    if (victim->type == MONS_SIXFIRHY)
    {
        if (!beam.is_tracer)
            victim->heal(10 + random2(15));
        // physical damage is still done
    }
    else
        dmg += 10 + random2(15);

    if (beam.is_tracer)
        return (false);

    if (you.can_see(victim))
    {
        if (victim->is_player())
            dmg_msg = "You are electrocuted!";
        else if (victim->type == MONS_SIXFIRHY)
            dmg_msg = victim->name(DESC_THE) + " is charged up!";
        else
            dmg_msg = "There is a sudden explosion of sparks!";
    }

    if (feat_is_water(grd(victim->pos())))
    {
        add_final_effect(FINEFF_LIGHTNING_DISCHARGE, beam.agent(), 0,
                         victim->pos(), 0);
    }

    return (false);
}

static bool _blessed_damages_victim(bolt &beam, actor* victim, int &dmg,
                                    std::string &dmg_msg)
{
    if (victim->undead_or_demonic())
    {
        dmg += 1 + (random2(dmg * 15) / 10);

        if (!beam.is_tracer && you.can_see(victim))
           dmg_msg = victim->name(DESC_THE) + " "
                   + victim->conj_verb("convulse") + "!";
    }

    return (false);
}

static int _blowgun_power_roll(bolt &beam)
{
    actor* agent = beam.agent();
    if (!agent)
        return 0;

    // Could check player shield skill here or something,
    // but that won't work with potential other sources
    // of reflection, and it doesn't matter anyway. [rob]
    if (beam.reflections > 0)
        return (agent->get_experience_level() / 3);

    int base_power;
    item_def* blowgun;
    if (agent->is_monster())
    {
        base_power = agent->get_experience_level();
        blowgun = agent->as_monster()->launcher();
    }
    else
    {
        base_power = agent->skill_rdiv(SK_THROWING);
        blowgun = agent->weapon();
    }

    ASSERT(blowgun && blowgun->sub_type == WPN_BLOWGUN);

    return (base_power + blowgun->plus);
}

static bool _blowgun_check(bolt &beam, actor* victim, bool message = true)
{
    if (victim->holiness() == MH_UNDEAD || victim->holiness() == MH_NONLIVING)
    {
        if (victim->is_monster())
            simple_monster_message(victim->as_monster(), " is unaffected.");
        else
            canned_msg(MSG_YOU_UNAFFECTED);
        return (false);
    }

    actor* agent = beam.agent();

    if (!agent || agent->is_monster() || beam.reflections > 0)
        return (true);

    const int skill = you.skill_rdiv(SK_THROWING);
    const item_def* wp = agent->weapon();
    ASSERT(wp && wp->sub_type == WPN_BLOWGUN);
    const int enchantment = wp->plus;

    // You have a really minor chance of hitting with no skills or good
    // enchants.
    if (victim->get_experience_level() < 15 && random2(100) <= 2)
        return (true);

    const int resist_roll = 2 + random2(4 + skill + enchantment);

    dprf("Brand rolled %d against defender HD: %d.",
         resist_roll, victim->get_experience_level());

    if (resist_roll < victim->get_experience_level())
    {
        if (victim->is_monster())
            simple_monster_message(victim->as_monster(), " resists.");
        else
            canned_msg(MSG_YOU_RESIST);
        return (false);
    }

    return (true);
}

static bool _paralysis_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->paralyse(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _sleep_hit_victim(bolt& beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->put_to_sleep(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _confusion_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->confuse(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _slow_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->slow_down(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _sickness_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->sicken(40 + random2(blowgun_power));
    return (true);
}

static bool _rage_hit_victim(bolt &beam, actor* victim, int dmg)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    if (victim->is_monster())
        victim->as_monster()->go_frenzy();
    else
        victim->go_berserk(false);

    return (true);
}

bool setup_missile_beam(const actor *agent, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning)
{
    dungeon_char_type zapsym = NUM_DCHAR_TYPES;
    switch (item.base_type)
    {
    case OBJ_WEAPONS:    zapsym = DCHAR_FIRED_WEAPON;  break;
    case OBJ_MISSILES:   zapsym = DCHAR_FIRED_MISSILE; break;
    case OBJ_ARMOUR:     zapsym = DCHAR_FIRED_ARMOUR;  break;
    case OBJ_WANDS:      zapsym = DCHAR_FIRED_STICK;   break;
    case OBJ_FOOD:       zapsym = DCHAR_FIRED_CHUNK;   break;
    case OBJ_SCROLLS:    zapsym = DCHAR_FIRED_SCROLL;  break;
    case OBJ_JEWELLERY:  zapsym = DCHAR_FIRED_TRINKET; break;
    case OBJ_POTIONS:    zapsym = DCHAR_FIRED_FLASK;   break;
    case OBJ_BOOKS:      zapsym = DCHAR_FIRED_BOOK;    break;
    case OBJ_STAVES:     zapsym = DCHAR_FIRED_STICK;   break;
    default: break;
    }

    beam.glyph = dchar_glyph(zapsym);
    beam.was_missile = true;

    item_def *launcher  = const_cast<actor*>(agent)->weapon(0);
    if (launcher && !item.launched_by(*launcher))
        launcher = NULL;

    int bow_brand = SPWPN_NORMAL;
    if (launcher != NULL)
        bow_brand = get_weapon_brand(*launcher);

    int ammo_brand = get_ammo_brand(item);

    // Launcher brand does not override ammunition except when elemental
    // opposites (which cancel).
    if (ammo_brand != SPMSL_NORMAL && bow_brand != SPWPN_NORMAL)
    {
        if (bow_brand == SPWPN_FLAME && ammo_brand == SPMSL_FROST ||
            bow_brand == SPWPN_FROST && ammo_brand == SPMSL_FLAME)
        {
            bow_brand = SPWPN_NORMAL;
            ammo_brand = SPMSL_NORMAL;
        }
        // Nessos gets to cheat.
        else if (agent->is_monster())
        {
            const monster* mon = static_cast<const monster* >(agent);
            if (mon->type != MONS_NESSOS)
                bow_brand = SPWPN_NORMAL;
        }
        else
            bow_brand = SPWPN_NORMAL;
    }

    if (is_launched(agent, launcher, item) == LRET_FUMBLED)
    {
        // We want players to actually carry blowguns and bows, not just rely
        // on high to-hit modifiers.  Rationalization: The poison/magic/
        // whatever is only applied to the tips.  -sorear

        bow_brand = SPWPN_NORMAL;
        ammo_brand = SPMSL_NORMAL;
    }

    // This is a bit of a special case because it applies even for melee
    // weapons, for which brand is normally ignored.
    returning = get_weapon_brand(item) == SPWPN_RETURNING
                || ammo_brand == SPMSL_RETURNING;

    if (agent->is_player())
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.beam_source   = NON_MONSTER;
        beam.smart_monster = true;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monster* mon = agent->as_monster();

        beam.attitude      = mons_attitude(mon);
        beam.beam_source   = mon->mindex();
        beam.smart_monster = (mons_intel(mon) >= I_NORMAL);
        beam.thrower       = KILL_MON_MISSILE;
    }

    beam.item         = &item;
    beam.effect_known = item_ident(item, ISFLAG_KNOW_TYPE);
    beam.source       = agent->pos();
    beam.colour       = item.colour;
    beam.flavour      = BEAM_MISSILE;
    beam.is_beam      = false;
    beam.aux_source.clear();

    beam.can_see_invis = agent->can_see_invisible();

    const unrandart_entry* entry = launcher && is_unrandom_artefact(*launcher)
        ? get_unrand_entry(launcher->special) : NULL;

    if (entry && entry->fight_func.launch)
    {
        setup_missile_type sm =
            entry->fight_func.launch(launcher, &beam, &ammo_name,
                                     &returning);

        switch (sm)
        {
        case SM_CONTINUE:
            break;
        case SM_FINISHED:
            return (false);
        case SM_CANCEL:
            return (true);
        }
    }

    bool poisoned   = (bow_brand == SPWPN_VENOM
                        || ammo_brand == SPMSL_POISONED);

    const bool exploding    = (ammo_brand == SPMSL_EXPLODING);
    const bool penetrating  = (bow_brand  == SPWPN_PENETRATION
                                || ammo_brand == SPMSL_PENETRATION);
    const bool silver       = (ammo_brand == SPMSL_SILVER);
    const bool disperses    = (ammo_brand == SPMSL_DISPERSAL);
    const bool charged      = bow_brand  == SPWPN_ELECTROCUTION;
    const bool blessed      = bow_brand == SPWPN_HOLY_WRATH;

    const bool paralysis    = ammo_brand == SPMSL_PARALYSIS;
    const bool slow         = ammo_brand == SPMSL_SLOW;
    const bool sleep        = ammo_brand == SPMSL_SLEEP;
    const bool confusion    = ammo_brand == SPMSL_CONFUSION;
    const bool sickness     = ammo_brand == SPMSL_SICKNESS;
    const bool rage         = ammo_brand == SPMSL_RAGE;

    ASSERT(!exploding || !is_artefact(item));

    beam.name = item.name(DESC_PLAIN, false, false, false);

    // Note that bow_brand is known since the bow is equipped.

    bool beam_changed = false;

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        // Chaos can't be poisoned, since that might conflict with
        // the random healing effect or overlap with the random
        // poisoning effect.
        poisoned = false;
        if (item.special == SPWPN_VENOM || item.special == SPMSL_CURARE)
            item.special = SPMSL_NORMAL;

        beam.effect_known = false;

        beam.flavour = BEAM_CHAOS;
        if (ammo_brand != SPMSL_CHAOS)
        {
            beam.name    += " of chaos";
            ammo_name    += " of chaos";
        }
        else
            beam_changed = true;
        beam.colour  = ETC_RANDOM;
    }
    else if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
             && ammo_brand != SPMSL_FROST && bow_brand != SPWPN_FROST)
    {
        beam.flavour = BEAM_FIRE;
        if (ammo_brand != SPMSL_FLAME)
        {
            beam.name    += " of flame";
            ammo_name    += " of flame";
        }
        else
            beam_changed = true;

        beam.colour  = RED;
    }
    else if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_FROST)
             && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
    {
        beam.flavour = BEAM_COLD;
        if (ammo_brand != SPMSL_FROST)
        {
            beam.name    += " of frost";
            ammo_name   += " of frost";
        }
        else
            beam_changed = true;
        beam.colour  = WHITE;
    }

    if (beam_changed)
        beam.name = item.name(DESC_PLAIN, false, false, false);

    ammo_name = item.name(DESC_PLAIN);

    ASSERT(beam.flavour == BEAM_MISSILE || !is_artefact(item));

    if (silver)
        beam.damage_funcs.push_back(_silver_damages_victim);
    if (poisoned)
        beam.hit_funcs.push_back(_poison_hit_victim);
    if (penetrating)
    {
        beam.range_funcs.push_back(_item_penetrates_victim);
        beam.hit_verb = "pierces through";
    }
    if (disperses)
        beam.hit_funcs.push_back(_dispersal_hit_victim);
    if (charged)
        beam.damage_funcs.push_back(_charged_damages_victim);
    if (blessed)
        beam.damage_funcs.push_back(_blessed_damages_victim);

    // New needle brands have no effect when thrown without launcher.
    if (launcher != NULL)
    {
        if (paralysis)
            beam.hit_funcs.push_back(_paralysis_hit_victim);
        if (slow)
            beam.hit_funcs.push_back(_slow_hit_victim);
        if (sleep)
            beam.hit_funcs.push_back(_sleep_hit_victim);
        if (confusion)
            beam.hit_funcs.push_back(_confusion_hit_victim);
        if (sickness)
            beam.hit_funcs.push_back(_sickness_hit_victim);
        if (rage)
            beam.hit_funcs.push_back(_rage_hit_victim);
    }

    if (disperses && item.special != SPMSL_DISPERSAL)
    {
        beam.name = "dispersing " + beam.name;
        ammo_name = "dispersing " + ammo_name;
    }

    // XXX: This doesn't make sense, but it works.
    if (poisoned && item.special != SPMSL_POISONED)
    {
        beam.name = "poisoned " + beam.name;
        ammo_name = "poisoned " + ammo_name;
    }

    if (penetrating && item.special != SPMSL_PENETRATION)
    {
        beam.name = "penetrating " + beam.name;
        ammo_name = "penetrating " + ammo_name;
    }

    if (silver && item.special != SPMSL_SILVER)
    {
        beam.name = "silvery " + beam.name;
        ammo_name = "silvery " + ammo_name;
    }

    if (blessed)
    {
        beam.name = "blessed " + beam.name;
        ammo_name = "blessed " + ammo_name;
    }

    // Do this here so that we get all the name mods except for a
    // redundant "exploding".
    if (exploding)
    {
         bolt *expl = new bolt(beam);

         expl->is_explosion = true;
         expl->damage       = dice_def(2, 5);
         expl->ex_size      = 1;

         if (beam.flavour == BEAM_MISSILE)
         {
             expl->flavour = BEAM_FRAG;
             expl->name   += " fragments";

             const std::string short_name =
                 item.name(DESC_PLAIN, false, false, false, false,
                           ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK
                           | ISFLAG_RACIAL_MASK);

             expl->name = replace_all(expl->name, item.name(DESC_PLAIN),
                                      short_name);
         }
         expl->name = "explosion of " + expl->name;

         beam.special_explosion = expl;
    }

    if (exploding && item.special != SPMSL_EXPLODING)
    {
        beam.name = "exploding " + beam.name;
        ammo_name = "exploding " + ammo_name;
    }

    if (beam.flavour != BEAM_MISSILE)
    {
        returning = false;

        beam.glyph = dchar_glyph(DCHAR_FIRED_BOLT);
    }

    if (!is_artefact(item))
        ammo_name = article_a(ammo_name, true);
    else
        ammo_name = "the " + ammo_name;

    return (false);
}

static int stat_adjust(int value, int stat, int statbase,
                       const int maxmult = 160, const int minmult = 40)
{
    int multiplier = (statbase + (stat - statbase) / 2) * 100 / statbase;
    if (multiplier > maxmult)
        multiplier = maxmult;
    else if (multiplier < minmult)
        multiplier = minmult;

    if (multiplier > 100)
        value = value * (100 + random2avg(multiplier - 100, 2)) / 100;
    else if (multiplier < 100)
        value = value * (100 - random2avg(100 - multiplier, 2)) / 100;

    return (value);
}

static int str_adjust_thrown_damage(int dam)
{
    return stat_adjust(dam, you.strength(), 15, 160, 90);
}

static int dex_adjust_thrown_tohit(int hit)
{
    return stat_adjust(hit, you.dex(), 13, 160, 90);
}

static void identify_floor_missiles_matching(item_def mitem, int idflags)
{
    mitem.flags &= ~idflags;

    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            for (stack_iterator si(coord_def(x,y)); si; ++si)
            {
                if ((si->flags & ISFLAG_THROWN) && items_stack(*si, mitem))
                    si->flags |= idflags;
            }
}

void merge_ammo_in_inventory(int slot)
{
    if (!you.inv[slot].defined())
        return;

    bool done_anything = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (i == slot || !you.inv[i].defined())
            continue;

        // Merge with the thrower slot. This could be a bad
        // thing if you're wielding IDed ammo and firing from
        // an unIDed stack...but that's a pretty remote case.
        if (items_stack(you.inv[i], you.inv[slot]))
        {
            if (!done_anything)
                mpr("You combine your ammunition.");

            inc_inv_item_quantity(slot, you.inv[i].quantity, true);
            dec_inv_item_quantity(i, you.inv[i].quantity);
            done_anything = true;
        }
    }
}

void throw_noise(actor* act, const bolt &pbolt, const item_def &ammo)
{
    const item_def* launcher = act->weapon();

    if (launcher == NULL || launcher->base_type != OBJ_WEAPONS)
        return;

    if (is_launched(act, launcher, ammo) != LRET_LAUNCHED)
        return;

    // Throwing and blowguns are silent...
    int         level = 0;
    const char* msg   = NULL;

    switch (launcher->sub_type)
    {
    case WPN_BLOWGUN:
        return;

    case WPN_SLING:
        level = 1;
        msg   = "You hear a whirring sound.";
        break;
     case WPN_BOW:
        level = 5;
        msg   = "You hear a twanging sound.";
        break;
     case WPN_LONGBOW:
        level = 6;
        msg   = "You hear a loud twanging sound.";
        break;
     case WPN_CROSSBOW:
        level = 7;
        msg   = "You hear a thunk.";
        break;

    default:
        die("Invalid launcher '%s'",
                 launcher->name(DESC_PLAIN).c_str());
        return;
    }
    if (act->is_player() || you.can_see(act))
        msg = NULL;

    noisy(level, act->pos(), msg, act->mindex());
}

// throw_it - currently handles player throwing only.  Monster
// throwing is handled in mon-act:_mons_throw()
// Note: If teleport is true, assume that pbolt is already set up,
// and teleport the projectile onto the square.
//
// Return value is only relevant if dummy_target is non-NULL, and returns
// true if dummy_target is hit.
bool throw_it(bolt &pbolt, int throw_2, bool teleport, int acc_bonus,
              dist *target)
{
    dist thr;
    int shoot_skill = 0;
    bool ammo_ided = false;

    int baseHit      = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;  // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;  // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0;    // 'extra' bonus from skill/dex/str
    int effSkill     = 0;        // effective launcher skill
    int dice_mult    = 100;
    bool returning   = false;    // Item can return to pack.
    bool did_return  = false;    // Returning item actually does return to pack.
    int slayDam      = 0;
    bool speed_brand = false;

    if (you.confused())
    {
        thr.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
        thr.isValid = true;
    }
    else if (target)
        thr = *target;
    else if (pbolt.target.zero())
    {
        direction_chooser_args args;
        args.mode = TARG_HOSTILE;
        direction(thr, args);

        if (!thr.isValid)
        {
            if (thr.isCancel)
                canned_msg(MSG_OK);

            return (false);
        }
    }
    pbolt.set_target(thr);

    item_def& thrown = you.inv[throw_2];
    ASSERT(thrown.defined());

    // Figure out if we're thrown or launched.
    const launch_retval projected = is_launched(&you, you.weapon(), thrown);

    // Making a copy of the item: changed only for venom launchers.
    item_def item = thrown;
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);

    // Items that get a temporary brand from a player spell lose the
    // brand as soon as the player lets go of the item.  Can't call
    // unwield_item() yet since the beam might get canceled.
    if (you.duration[DUR_WEAPON_BRAND] && projected != LRET_LAUNCHED
        && throw_2 == you.equip[EQ_WEAPON])
    {
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
    }

    std::string ammo_name;

    if (setup_missile_beam(&you, pbolt, item, ammo_name, returning))
    {
        you.turn_is_over = false;
        return (false);
    }

    // Did we know the ammo's brand before throwing it?
    const bool ammo_brand_known = item_type_known(thrown);

    // Get the ammo/weapon type.  Convenience.
    const object_class_type wepClass = thrown.base_type;
    const int               wepType  = thrown.sub_type;

    // Determine range.
    int max_range = 0;
    int range = 0;

    if (projected)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            range     = 1 + random2(you.strength() / 5);
            max_range = you.strength() / 5;
            if (you.can_throw_large_rocks())
            {
                range     += random_range(4, 7);
                max_range += 7;
            }
        }
        else if (wepType == MI_THROWING_NET)
            max_range = range = 2 + you.body_size(PSIZE_BODY);
        else
        {
            max_range = range = LOS_RADIUS;
        }
    }
    else
    {
        // Range based on mass & strength, between 1 and 9.
        max_range = range = std::max(you.strength()-item_mass(thrown)/10 + 3, 1);
    }

    range = std::min(range, LOS_RADIUS);
    max_range = std::min(max_range, LOS_RADIUS);

    // For the tracer, use max_range. For the actual shot, use range.
    pbolt.range = max_range;

    // Save the special explosion (exploding missiles) for later.
    // Need to clear this if unknown to avoid giving away the explosion.
    bolt* expl = pbolt.special_explosion;
    if (!pbolt.effect_known)
        pbolt.special_explosion = NULL;

    // Don't do the tracing when using Portaled Projectile, or when confused.
    if (!teleport && !you.confused())
    {
        // Set values absurdly high to make sure the tracer will
        // complain if we're attempting to fire through allies.
        pbolt.hit    = 100;
        pbolt.damage = dice_def(1, 100);

        // Init tracer variables.
        pbolt.foe_info.reset();
        pbolt.friend_info.reset();
        pbolt.foe_ratio = 100;
        pbolt.is_tracer = true;

        pbolt.fire();

        // Should only happen if the player answered 'n' to one of those
        // "Fire through friendly?" prompts.
        if (pbolt.beam_cancelled)
        {
            canned_msg(MSG_OK);
            you.turn_is_over = false;
            if (pbolt.special_explosion != NULL)
                delete pbolt.special_explosion;
            return (false);
        }
        pbolt.hit    = 0;
        pbolt.damage = dice_def();
    }
    pbolt.is_tracer = false;

    // Reset values.
    pbolt.range = range;
    pbolt.special_explosion = expl;

    bool unwielded = false;
    if (throw_2 == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        if (!wield_weapon(true, SLOT_BARE_HANDS, true, false, false))
            return (false);

        unwielded = true;
    }

    // Now start real firing!
    origin_set_unknown(item);

    if (is_blood_potion(item) && thrown.quantity > 1)
    {
        // Initialise thrown potion with oldest potion in stack.
        long val = remove_oldest_blood_potion(thrown);
        val -= you.num_turns;
        item.props.clear();
        init_stack_blood_potions(item, val);
    }

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if (!teleport)
        pbolt.set_target(thr);

    // baseHit and damage for generic objects
    baseHit = std::min(0, you.strength() - item_mass(item) / 10);
    baseDam = item_mass(item) / 100;

    // special: might be throwing generic weapon;
    // use base wep. damage, w/ penalty
    if (wepClass == OBJ_WEAPONS)
        baseDam = std::max(0, property(item, PWPN_DAMAGE) - 4);

    // Extract weapon/ammo bonuses due to magic.
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    int bow_brand = SPWPN_NORMAL;

    if (projected == LRET_LAUNCHED)
        bow_brand = get_weapon_brand(*you.weapon());

    int ammo_brand = get_ammo_brand(item);

    if (projected == LRET_FUMBLED)
    {
        // See comment in setup_missile_beam.  Why is this duplicated?
        ammo_brand = SPMSL_NORMAL;
    }

    // CALCULATIONS FOR LAUNCHED WEAPONS
    if (projected == LRET_LAUNCHED)
    {
        const item_def &launcher = *you.weapon();

        // Extract launcher bonuses due to magic.
        lnchHitBonus = launcher.plus;
        lnchDamBonus = launcher.plus2;

        const int item_base_dam = property(item, PWPN_DAMAGE);
        const int lnch_base_dam = property(launcher, PWPN_DAMAGE);

        const skill_type launcher_skill = range_skill(launcher);

        baseHit = property(launcher, PWPN_HIT);
        baseDam = lnch_base_dam + random2(1 + item_base_dam);

        // Slings are terribly weakened otherwise.
        if (lnch_base_dam == 0)
            baseDam = item_base_dam;

        // If we've a zero base damage + an elemental brand, up the damage
        // slightly so the brand has something to work with. This should
        // only apply to needles.
        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to consider
        // this beam "needle-like". (XXX)
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        dprf("Base hit == %d; Base damage == %d "
                "(item %d + launcher %d)",
                        baseHit, baseDam,
                        item_base_dam, lnch_base_dam);

        // Fix ammo damage bonus, since missiles only use inv_plus.
        ammoDamBonus = ammoHitBonus;

        // Check for matches; dwarven, elven, orcish.
        if (!(get_equip_race(*you.weapon()) == 0))
        {
            if (get_equip_race(*you.weapon()) == get_equip_race(item))
            {
                baseHit++;
                baseDam++;

                // elves with elven bows
                if (get_equip_race(*you.weapon()) == ISFLAG_ELVEN
                    && player_genus(GENPC_ELVEN))
                {
                    baseHit++;
                }
            }
        }

        // Lower accuracy if held in a net.
        if (you.attribute[ATTR_HELD])
            baseHit = baseHit / 2 - 1;

        // For all launched weapons, maximum effective specific skill
        // is twice throwing skill.  This models the fact that no matter
        // how 'good' you are with a bow, if you know nothing about
        // trajectories you're going to be a damn poor bowman.  Ditto
        // for crossbows and slings.

        // [dshaligram] Throwing now two parts launcher skill, one part
        // ranged combat. Removed the old model which is... silly.

        // [jpeg] Throwing now only affects actual throwing weapons,
        // i.e. not launched ones. (Sep 10, 2007)

        shoot_skill = you.skill_rdiv(launcher_skill);
        effSkill    = shoot_skill;

        const int speed = launcher_final_speed(launcher, you.shield());
        dprf("Final launcher speed: %d", speed);
        you.time_taken = div_rand_round(speed * you.time_taken, 100);

        // [dshaligram] Improving missile weapons:
        //  - Remove the strength/enchantment cap where you need to be strong
        //    to exploit a launcher bonus.
        //  - Add on launcher and missile pluses to extra damage.

        // [dshaligram] This can get large...
        exDamBonus = lnchDamBonus + random2(1 + ammoDamBonus);
        exDamBonus = (exDamBonus > 0   ? random2(exDamBonus + 1)
                                       : -random2(-exDamBonus + 1));
        exHitBonus = (lnchHitBonus > 0 ? random2(lnchHitBonus + 1)
                                       : -random2(-lnchHitBonus + 1));

        practise(EX_WILL_LAUNCH, launcher_skill);
        count_action(CACT_FIRE, launcher.sub_type);

        // Removed 2 random2(2)s from each of the learning curves, but
        // left slings because they're hard enough to develop without
        // a good source of shot in the dungeon.
        switch (launcher_skill)
        {
        case SK_SLINGS:
        {
            // Sling bullets are designed for slinging and easier to aim.
            if (wepType == MI_SLING_BULLET)
                baseHit += 4;

            exHitBonus += (effSkill * 3) / 2;

            // Strength is good if you're using a nice sling.
            int strbonus = (10 * (you.strength() - 10)) / 9;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // cap
            strbonus = std::min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;
            // Add skill for slings... helps to find those vulnerable spots.
            dice_mult = dice_mult * (14 + random2(1 + effSkill)) / 14;

            // Now kill the launcher damage bonus.
            lnchDamBonus = std::min(0, lnchDamBonus);
            break;
        }

        // Blowguns take a _very_ steady hand; a lot of the bonus
        // comes from dexterity.  (Dex bonus here as well as below.)
        case SK_THROWING:
            baseHit -= 2;
            exHitBonus += (effSkill * 3) / 2 + you.dex() / 2;

            // No extra damage for blowguns.
            // exDamBonus = 0;

            // Now kill the launcher damage and ammo bonuses.
            lnchDamBonus = std::min(0, lnchDamBonus);
            ammoDamBonus = std::min(0, ammoDamBonus);
            break;

        case SK_BOWS:
        {
            baseHit -= 3;
            exHitBonus += (effSkill * 2);

            // Strength is good if you're using a nice bow.
            int strbonus = (10 * (you.strength() - 10)) / 4;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // Cap; reduced this cap, because we don't want to allow
            // the extremely-strong to quadruple the enchantment bonus.
            strbonus = std::min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;

            // Add in skill for bows - helps you to find those vulnerable spots.
            // exDamBonus += effSkill;

            dice_mult = dice_mult * (17 + random2(1 + effSkill)) / 17;

            // Now kill the launcher damage bonus.
            lnchDamBonus = std::min(0, lnchDamBonus);
            break;
        }
            // Crossbows are easy for unskilled people.

        case SK_CROSSBOWS:
            baseHit++;
            exHitBonus += (3 * effSkill) / 2 + 6;
            // exDamBonus += effSkill * 2 / 3 + 4;

            dice_mult = dice_mult * (22 + random2(1 + effSkill)) / 22;

        default:
            break;
        }

        if (bow_brand == SPWPN_VORPAL)
        {
            // Vorpal brand adds 20% damage bonus. Decreased from 30% to
            // keep it more comparable with speed brand after the speed nerf.
            dice_mult = dice_mult * 120 / 100;
        }

        // Note that branded missile damage goes through defender
        // resists.
        if (ammo_brand == SPMSL_STEEL)
            dice_mult = dice_mult * 130 / 100;

        if (elemental_missile_beam(bow_brand, ammo_brand))
            dice_mult = dice_mult * 140 / 100;

        // ID check. Can't ID off teleported projectiles, uh, because
        // it's too weird. Also it messes up the messages.
        if (item_ident(*you.weapon(), ISFLAG_KNOW_PLUSES))
        {
            if (!teleport
                && !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES)
                && x_chance_in_y(shoot_skill, 100))
            {
                set_ident_flags(item, ISFLAG_KNOW_PLUSES);
                set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_PLUSES);
                ammo_ided = true;
                identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
                mprf("You are firing %s.",
                     you.inv[throw_2].name(DESC_A).c_str());
            }
        }
        else if (!teleport && x_chance_in_y(shoot_skill, 100))
        {
            item_def& weapon = *you.weapon();
            set_ident_flags(weapon, ISFLAG_KNOW_PLUSES);

            mprf("You are wielding %s.", weapon.name(DESC_A).c_str());

            more();
            you.wield_change = true;
        }

        if (get_weapon_brand(launcher) == SPWPN_SPEED)
            speed_brand = true;
    }

    // check for returning ammo from launchers
    if (returning && projected == LRET_LAUNCHED)
    {
        if (!x_chance_in_y(1, 1 + skill_bump(range_skill(*you.weapon()))))
            did_return = true;
    }

    // CALCULATIONS FOR THROWN WEAPONS
    if (projected == LRET_THROWN)
    {
        returning = returning && !teleport;

        if (returning && !x_chance_in_y(1, 1 + skill_bump(SK_THROWING)))
            did_return = true;

        baseHit = 0;

        // Missiles only use inv_plus.
        if (wepClass == OBJ_MISSILES)
            ammoDamBonus = ammoHitBonus;

        // All weapons that use 'throwing' go here.
        if (wepClass == OBJ_WEAPONS
            || (wepClass == OBJ_MISSILES
                && (wepType == MI_STONE || wepType == MI_LARGE_ROCK
                    || wepType == MI_DART || wepType == MI_JAVELIN)))
        {
            // Elves with elven weapons.
            if (get_equip_race(item) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
            {
                baseHit++;
            }

            // Give an appropriate 'tohit':
            // * clubs and hand axes are -5
            // * spears are -1
            // * large rocks, stones and throwing nets are 0
            // * daggers and javelins are +1
            // * darts are +2
            if (wepClass == OBJ_WEAPONS)
            {
                switch (wepType)
                {
                    case WPN_DAGGER:
                        baseHit++;
                        break;
                    case WPN_SPEAR:
                        baseHit--;
                        break;
                    default:
                        baseHit -= 5;
                        break;
                }
            }
            else if (wepClass == OBJ_MISSILES)
            {
                switch (wepType)
                {
                    case MI_DART:
                        baseHit += 2;
                        break;
                    case MI_JAVELIN:
                        baseHit++;
                        break;
                    default:
                        break;
                }
            }

            exHitBonus = you.skill(SK_THROWING, 2);

            baseDam = property(item, PWPN_DAMAGE);

            // [dshaligram] The defined base damage applies only when used
            // for launchers. Hand-thrown stones do only half
            // base damage. Yet another evil 4.0ism.
            if (wepClass == OBJ_MISSILES && wepType == MI_STONE)
                baseDam = div_rand_round(baseDam, 2);

            // Dwarves/orcs with dwarven/orcish weapons.
            if (get_equip_race(item) == ISFLAG_DWARVEN
                   && you.species == SP_DEEP_DWARF
                || get_equip_race(item) == ISFLAG_ORCISH
                   && you.species == SP_HILL_ORC)
            {
                baseDam++;
            }

            exDamBonus = (you.skill(SK_THROWING, 5) + you.strength() * 10 - 100)
                       / 12;

            // Now, exDamBonus is a multiplier.  The full multiplier
            // is applied to base damage, but only a third is applied
            // to the magical modifier.
            exDamBonus = (exDamBonus * (3 * baseDam + ammoDamBonus)) / 30;
        }

        if (wepClass == OBJ_MISSILES)
        {
            switch (wepType)
            {
            case MI_LARGE_ROCK:
                if (you.can_throw_large_rocks())
                    baseHit = 1;
                break;

            case MI_DART:
                // Darts also using throwing skills, now.
                exHitBonus += skill_bump(SK_THROWING);
                exDamBonus += you.skill(SK_THROWING, 3) / 5;
                break;

            case MI_JAVELIN:
                // Javelins use throwing skill.
                exHitBonus += skill_bump(SK_THROWING);
                exDamBonus += you.skill(SK_THROWING, 3) / 5;

                // Adjust for strength and dex.
                exDamBonus = str_adjust_thrown_damage(exDamBonus);
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

                // High dex helps damage a bit, too (aim for weak spots).
                exDamBonus = stat_adjust(exDamBonus, you.dex(), 20, 150, 100);
                break;

            case MI_THROWING_NET:
                // Nets use throwing skill.  They don't do any damage!
                baseDam = 0;
                exDamBonus = 0;
                ammoDamBonus = 0;

                // ...but accuracy is important for this one.
                baseHit = 1;
                exHitBonus += skill_bump(SK_THROWING, 7) / 2;
                // Adjust for strength and dex.
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);
                break;
            }

            if (ammo_brand == SPMSL_STEEL)
                dice_mult = dice_mult * 130 / 100;

            practise(EX_WILL_THROW_MSL, wepType);
            count_action(CACT_THROW, wepType);
        }
        else
        {
            practise(EX_WILL_THROW_WEAPON);
        }

        // ID check
        if (!teleport
            && !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES)
            && item.base_type == OBJ_MISSILES ?
               x_chance_in_y(you.skill(SK_THROWING, 100), 10000) :
               maybe_id_weapon(item))
        {
            set_ident_flags(item, ISFLAG_KNOW_PLUSES);
            set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_PLUSES);
            identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
            ammo_ided = true;
            mprf("You are throwing %s.",
                 you.inv[throw_2].name(DESC_A).c_str());
        }
    }

    // Dexterity bonus, and possible skill increase for silly throwing.
    if (projected)
    {
        if (wepType != MI_LARGE_ROCK && wepType != MI_THROWING_NET)
        {
            exHitBonus += you.dex() / 2;

            // slaying bonuses
            if (wepType != MI_NEEDLE)
            {
                slayDam = slaying_bonus(PWPN_DAMAGE, true);
                slayDam = (slayDam < 0 ? -random2(1 - slayDam)
                                       :  random2(1 + slayDam));
            }

            exHitBonus += slaying_bonus(PWPN_HIT, true);
        }
    }
    else // LRET_FUMBLED
    {
        practise(EX_WILL_THROW_OTHER);

        exHitBonus = you.dex() / 4;
    }

    // FINALISE tohit and damage
    if (exHitBonus >= 0)
        pbolt.hit = baseHit + random2avg(exHitBonus + 1, 2);
    else
        pbolt.hit = baseHit - random2avg(0 - (exHitBonus - 1), 2);

    if (exDamBonus >= 0)
        pbolt.damage = dice_def(1, baseDam + random2(exDamBonus + 1));
    else
        pbolt.damage = dice_def(1, baseDam - random2(0 - (exDamBonus - 1)));

    pbolt.damage.size  = dice_mult * pbolt.damage.size / 100;
    pbolt.damage.size += slayDam;

    // Only add bonuses if we're throwing something sensible.
    if (projected || wepClass == OBJ_WEAPONS)
    {
        pbolt.hit += ammoHitBonus + lnchHitBonus;
        pbolt.damage.size += ammoDamBonus + lnchDamBonus;
    }

    if (speed_brand)
        pbolt.damage.size = div_rand_round(pbolt.damage.size * 9, 10);

    // Add in bonus (only from Portal Projectile for now).
    if (acc_bonus != DEBUG_COOKIE)
        pbolt.hit += acc_bonus;

    scale_dice(pbolt.damage);

    dprf("H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size);

    // Create message.
    mprf("%s %s%s %s.",
          teleport  ? "Magically, you" : "You",
          projected ? "" : "awkwardly ",
          projected == LRET_LAUNCHED ? "shoot" : "throw",
          ammo_name.c_str());

    // Ensure we're firing a 'missile'-type beam.
    pbolt.is_beam   = false;
    pbolt.is_tracer = false;

    pbolt.loudness = int(sqrt(item_mass(item))/3 + 0.5);

    // Mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    bool hit = false;
    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        pbolt.use_target_as_pos = true;
        pbolt.affect_cell();
        pbolt.affect_endpoint();
        if (!did_return && acc_bonus != DEBUG_COOKIE)
            pbolt.drop_object();
    }
    else
    {
        if (crawl_state.game_is_hints())
            Hints.hints_throw_counter++;

        // Dropping item copy, since the launched item might be different.
        pbolt.drop_item = !did_return;
        pbolt.fire();

        hit = !pbolt.hit_verb.empty();

        // The item can be destroyed before returning.
        if (did_return && thrown_object_destroyed(&item, pbolt.target))
            did_return = false;
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        did_god_conduct(DID_CHAOS, 2 + random2(3),
                        bow_brand == SPWPN_CHAOS || ammo_brand_known);
    }

    if (bow_brand == SPWPN_SPEED)
        did_god_conduct(DID_HASTY, 1, true);

    if (ammo_brand == SPMSL_RAGE)
        did_god_conduct(DID_HASTY, 6 + random2(3), ammo_brand_known);

    if (did_return)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow();
        pbolt.fire();

        msg::stream << item.name(DESC_THE) << " returns to your pack!"
                    << std::endl;

        // Player saw the item return.
        if (!is_artefact(you.inv[throw_2]))
        {
            // Since this only happens for non-artefacts, also mark properties
            // as known.
            set_ident_flags(you.inv[throw_2],
                            ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);
        }
    }
    else
    {
        // Should have returned but didn't.
        if (returning && item_type_known(you.inv[throw_2]))
        {
            msg::stream << item.name(DESC_THE)
                        << " fails to return to your pack!" << std::endl;
        }
        dec_inv_item_quantity(throw_2, 1);
        if (unwielded)
            canned_msg(MSG_EMPTY_HANDED_NOW);
    }

    throw_noise(&you, pbolt, thrown);

    // ...any monster nearby can see that something has been thrown, even
    // if it didn't make any noise.
    alert_nearby_monsters();

    if (ammo_ided)
        merge_ammo_in_inventory(throw_2);

    you.turn_is_over = true;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return (hit);
}

bool thrown_object_destroyed(item_def *item, const coord_def& where)
{
    ASSERT(item != NULL);

    std::string name = item->name(DESC_PLAIN, false, true, false);

    if (item->base_type != OBJ_MISSILES)
        return (false);

    int brand = get_ammo_brand(*item);
    if (brand == SPMSL_CHAOS || brand == SPMSL_DISPERSAL || brand == SPMSL_EXPLODING)
        return (true);

    // Nets don't get destroyed by throwing.
    if (item->sub_type == MI_THROWING_NET)
        return (false);

    int chance;

    // [dshaligram] Removed influence of Throwing on ammo preservation.
    // The effect is nigh impossible to perceive.
    switch (item->sub_type)
    {
    case MI_NEEDLE:
        chance = (brand == SPMSL_CURARE ? 3 : 6);
        break;

    case MI_SLING_BULLET:
    case MI_STONE:
    case MI_ARROW:
    case MI_BOLT:
        chance = 4;
        break;

    case MI_DART:
        chance = 3;
        break;

    case MI_JAVELIN:
        chance = 10;
        break;

    case MI_LARGE_ROCK:
        chance = 25;
        break;

    default:
        die("Unknown missile type");
    }

    // Inflate by 4 to avoid rounding errors.
    const int mult = 4;
    chance *= mult;

    if (brand == SPMSL_STEEL)
        chance *= 10;
    if (brand == SPMSL_FLAME)
        chance /= 2;
    if (brand == SPMSL_FROST)
        chance /= 2;

    // Enchanted projectiles get an extra shot at avoiding
    // destruction: plus / (3 + plus) chance of survival.
    return (x_chance_in_y(mult, chance) && x_chance_in_y(3, item->plus + 3));
}
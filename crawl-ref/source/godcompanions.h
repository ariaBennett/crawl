/**
 * @file
 * @brief Tracking permaallies granted by Yred and Beogh.
**/

#ifndef GOD_COMPANION_H
#define GOD_COMPANION_H

#include "monster.h"
#include "mon-transit.h"
#include <map>
#include <list>

struct companion
{
    follower mons;
    level_id level;
    int timestamp;

    companion() : mons(), level() { }
    companion(const monster& m);
};

extern map<mid_t, companion> companion_list;

void add_companion(monster* mons);
void remove_companion(monster* mons);
void remove_all_companions(god_type god);
void move_companion_to(const monster* mons, const level_id lid);

void update_companions();

bool recall_offlevel_companions();

void list_companions();
bool companion_is_elsewhere(mid_t mid, bool must_exist = false);

void populate_offlevel_recall_list(vector<pair<mid_t, int> > &recall_list);
bool recall_offlevel_ally(mid_t mid);

void wizard_list_companions();

#if TAG_MAJOR_VERSION == 34
void fixup_bad_companions();
#endif

#endif

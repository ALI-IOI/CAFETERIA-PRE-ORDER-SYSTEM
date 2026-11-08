// Order tokens: the number a person shows at the counter, as at a restaurant.
// Every order (one reservation, one whole tray, or one waitlist promotion) gets
// the next token of the day; all dishes of the order share it.
//
// Grouping uses a direct-address array: tokens are 1 .. nextToken-1, so the
// array slot of a token is its number and adding a dish to its group is O(1).
// One pass over the reservation list fills it: O(n + t). The board's ready
// column is then ordered by when each token became ready (insertion sort, as
// the staff reservation list does), which is the order the counter calls them.
#include "core/cafeteria.h"
#include <cstdio>

std::string tokenText(int token) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%03d", token);
    return buf;
}

TokenState tokenState(const TokenInfo& t) {
    if (t.cooking > 0) return TokenState::Cooking;
    if (t.ready > 0) return TokenState::Ready;
    return TokenState::Done;
}

int todaysTokens(const Cafeteria& caf, TokenInfo* out, int cap) {
    const int slots = caf.nextToken;  // tokens 1 .. nextToken-1
    if (slots <= 1 || cap <= 0) return 0;
    TokenInfo* byToken = new TokenInfo[slots];
    for (int t = 0; t < slots; t++) {
        byToken[t].token = t;
        byToken[t].dishes = byToken[t].cooking = byToken[t].ready = byToken[t].done = 0;
        byToken[t].readyAt = 0;
    }
    const int today = caf.currentDay;
    for (const Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->day != today || r->token <= 0 || r->token >= slots || r->status == ResStatus::Cancelled) continue;
        TokenInfo& t = byToken[r->token];
        t.user = r->user;
        t.dishes++;
        if (r->status == ResStatus::Reserved) t.cooking++;
        else if (r->status == ResStatus::Ready) t.ready++;
        else t.done++;
        if (r->readyAt > t.readyAt) t.readyAt = r->readyAt;
    }
    int n = 0;
    for (int t = 1; t < slots && n < cap; t++) {
        if (byToken[t].dishes == 0) continue;  // every dish of it was cancelled
        if (byToken[t].cooking > 0) byToken[t].readyAt = 0;  // not ready until all of it is
        out[n++] = byToken[t];
    }
    delete[] byToken;
    return n;
}

void tokenBoard(const Cafeteria& caf, TokenInfo* cooking, int& nCooking, TokenInfo* ready, int& nReady, int cap) {
    nCooking = nReady = 0;
    if (cap <= 0) return;
    TokenInfo* all = new TokenInfo[cap];
    int n = todaysTokens(caf, all, cap);
    for (int i = 0; i < n; i++) {
        TokenState s = tokenState(all[i]);
        if (s == TokenState::Cooking) {
            cooking[nCooking++] = all[i];  // already ascending
        } else if (s == TokenState::Ready) {
            // insertion sort by the time the token became ready, then by number
            int j = nReady++;
            while (j > 0 && (ready[j - 1].readyAt > all[i].readyAt ||
                             (ready[j - 1].readyAt == all[i].readyAt && ready[j - 1].token > all[i].token))) {
                ready[j] = ready[j - 1];
                j--;
            }
            ready[j] = all[i];
        }
    }
    delete[] all;
}

bool findToken(const Cafeteria& caf, int token, TokenInfo& out) {
    if (token <= 0 || token >= caf.nextToken) return false;
    const int cap = caf.nextToken;
    TokenInfo* all = new TokenInfo[cap];
    int n = todaysTokens(caf, all, cap);
    bool found = false;
    for (int i = 0; i < n && !found; i++) {
        if (all[i].token == token) {
            out = all[i];
            found = true;
        }
    }
    delete[] all;
    return found;
}

Result handOverToken(Cafeteria& caf, const std::string& staff, int token, int& handedOver, bool& undoRecorded) {
    handedOver = 0;
    undoRecorded = true;
    cafEnsureToday(caf);
    TokenInfo t;
    if (!findToken(caf, token, t)) return Result::NoSuchToken;
    if (t.ready == 0 && t.cooking == 0) return Result::TokenDone;
    if (t.ready == 0) return Result::TokenNotReady;
    for (Reservation* r = caf.reservations.head; r != nullptr; r = r->next) {
        if (r->day != caf.currentDay || r->token != token || r->status != ResStatus::Ready) continue;
        bool rec = false;
        if (markCollected(caf, staff, r->id, rec) == Result::Ok) {
            handedOver++;
            if (!rec) undoRecorded = false;
        }
    }
    return Result::Ok;
}

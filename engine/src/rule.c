#include "types.h"

Rule rule_from_gomocup(int r) {
    Rule rule = {0};
    rule.gomocup_rule = r;
    if (r & 4) {            // renju
        rule.black_exact_five = 1;
        rule.forbid_overline = 1;
        rule.forbid_44 = 1;
        rule.forbid_33 = 1;
    } else if (r & 1) {     // standard (ちょうど五)
        rule.black_exact_five = 1;
        rule.white_exact_five = 1;
    }
    return rule;
}

// 授業審判: 白・黒とも五連以上で勝ち (lenCheck)。ただし check の長連ビットが
// 立っていれば黒の 6 連以上は禁じ手 (五より先に判定される)
Rule rule_from_class_check(int check) {
    Rule rule = {0};
    rule.gomocup_rule = 4;
    rule.forbid_33 = (check & 1) != 0;
    rule.forbid_44 = (check & 2) != 0;
    rule.forbid_overline = (check & 4) != 0;
    rule.black_exact_five = rule.forbid_overline;
    return rule;
}

int rule_has_forbidden(const Rule *r) {
    return r->forbid_33 || r->forbid_44 || r->forbid_overline;
}

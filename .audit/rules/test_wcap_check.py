#!/usr/bin/env python3
"""
Self-test for the WBP-1998 checks in wcap-check.py.

Each check has a positive case it must fire on and a negative case it must stay silent
on, taken from the shapes seen while calibrating against wax-system-contracts at 715ddba
(the 2026-09 audit base) and at the remediated tree. A rule that fires on the negative
case has lost the precision that justifies its place in the ruleset; see .audit/README.md.

    python3 .audit/rules/test_wcap_check.py
"""

import importlib.util
import pathlib
import unittest

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('wcap_check', HERE / 'wcap-check.py')
wc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wc)


def wrap(body, sig='void system_contract::act( uint32_t n, const std::string& s, const std::vector<std::string>& members )'):
    return f'namespace x {{\n   {sig} {{\n{body}\n   }}\n}}\n'


def classes(rows):
    return [cls for _, cls, _ in rows]


class C6Message(unittest.TestCase):
    def test_strict_wording_nonstrict_operator_fires(self):          # WCAP-SYS-2026-002
        rows = wc.check_c6('t.cpp', wrap('check(n >= 0, "n must be greater than 0");'))
        self.assertEqual(classes(rows), ['C6'])

    def test_nonstrict_wording_strict_operator_fires(self):
        rows = wc.check_c6('t.cpp', wrap('check(n > 0, "n must be at least 0");'))
        self.assertEqual(classes(rows), ['C6'])

    def test_literal_mismatch_fires(self):                            # 715ddba wps.cpp:223
        rows = wc.check_c6('t.cpp', wrap('check(s.size() < 400, "subtitle should be shorter than 256 characters.");'))
        self.assertEqual(classes(rows), ['C6'])

    def test_vector_counted_as_characters_fires(self):                # 715ddba wps.cpp:228
        rows = wc.check_c6('t.cpp', wrap('check(members.size() < 50, "members should be shorter than 50 characters.");'))
        self.assertEqual(classes(rows), ['C6'])

    def test_consistent_requirement_is_silent(self):
        body = ('check(n > 0, "n must be greater than 0");\n'
                'check(n >= 30, "n should be at least 30 days");\n'
                'check(s.size() < 256, "title should be shorter than 256 characters.");\n'
                'check(n <= 21, "n must be less than or equal to 21");')
        self.assertEqual(wc.check_c6('t.cpp', wrap(body)), [])

    def test_negated_and_failure_forms_are_silent(self):
        body = ('check(n <= 500, "n cannot exceed 500");\n'
                'check(n >= 1, "n can\'t shrink below 1");\n'
                'check(s.size() <= 256, "memo has more than 256 bytes");\n'
                'check(n > 0, "calculated fee is below minimum");\n'
                'check(n >= 600, "cooldown must be at least 600 (10 minutes)");\n'
                'check(n >= 15, "cannot claim until the chain is activated (at least 15% of tokens vote)");')
        self.assertEqual(wc.check_c6('t.cpp', wrap(body)), [])

    def test_negated_wrong_strictness_fires(self):
        rows = wc.check_c6('t.cpp', wrap('check(n < 500, "n cannot exceed 500");'))
        self.assertEqual(classes(rows), ['C6'])

    def test_compound_conditions_are_skipped(self):
        self.assertEqual(wc.check_c6('t.cpp', wrap('check(n >= 1 && n <= 21, "n must be greater than 0");')), [])

    def test_concatenated_message_uses_literal_prefix(self):
        rows = wc.check_c6('t.cpp', wrap('check(n <= max_n, "n cannot exceed " + std::to_string(max_n));'))
        self.assertEqual(rows, [])


class C6Sibling(unittest.TestCase):
    PAIR = ('void system_contract::regthing( name a, const std::string& t, uint64_t d ) {{\n{0}\n}}\n'
            'void system_contract::editthing( name a, const std::string& t, uint64_t d ) {{\n{1}\n}}\n')
    REG = 'check(t.size() > 0, "t");\ncheck(t.size() < 256, "t");\ncheck(d >= 30, "d at least 30");'

    def test_divergent_pair_fires_with_the_difference(self):           # WCAP-SYS-2026-003
        edit = 'check(t.size() > 0, "t");\ncheck(t.size() < 256, "t");\ncheck(d > 0, "d longer than 0");'
        rows = wc.check_c6_sibling('t.cpp', self.PAIR.format(self.REG, edit))
        self.assertEqual(classes(rows), ['C6-sibling'])
        self.assertIn('d >= 30', rows[0][2])

    def test_message_drift_fires(self):                               # regcommittee/edcommittee
        edit = self.REG.replace('"d at least 30"', '"d must be at least 30"')
        rows = wc.check_c6_sibling('t.cpp', self.PAIR.format(self.REG, edit))
        self.assertIn('drifted', rows[0][2])

    def test_identical_copies_still_warn(self):
        rows = wc.check_c6_sibling('t.cpp', self.PAIR.format(self.REG, self.REG))
        self.assertIn('agree today', rows[0][2])

    def test_shared_helper_is_silent(self):
        reg = 'validate_thing_fields( t, d );'
        self.assertEqual(wc.check_c6_sibling('t.cpp', self.PAIR.format(reg, reg)), [])

    def test_lookup_checks_do_not_count(self):
        reg = 'auto itr = _t.find(a.value);\ncheck(itr == _t.end(), "exists");'
        edit = 'auto itr = _t.find(a.value);\ncheck(itr != _t.end(), "missing");'
        self.assertEqual(wc.check_c6_sibling('t.cpp', self.PAIR.format(reg, edit)), [])


class C6Setter(unittest.TestCase):
    def test_unvalidated_store_fires(self):                           # WCAP-SYS-2026-005
        rows = wc.check_c6_setter('t.cpp', wrap('require_auth(get_self());\n_gstate6.bp_default_score = n;',
                                                sig='void system_contract::setbpdefscore( uint32_t n )'))
        self.assertEqual(classes(rows), ['C6-setter'])

    def test_bounded_store_is_silent(self):
        rows = wc.check_c6_setter('t.cpp', wrap('check( n > 0, "n must be greater than 0" );\n_gstate6.scale = n;',
                                                sig='void system_contract::setbpscale( uint32_t n )'))
        self.assertEqual(rows, [])

    def test_non_setter_and_non_numeric_are_silent(self):
        rows = wc.check_c6_setter('t.cpp', wrap('_gstate6.x = n;', sig='void system_contract::regfoo( uint32_t n )'))
        rows += wc.check_c6_setter('t.cpp', wrap('_gstate6.c = c;', sig='void system_contract::setguildcont( name c )'))
        self.assertEqual(rows, [])


class C3Asset(unittest.TestCase):
    def test_unvalidated_asset_fires(self):                           # WCAP-SYS-2026-004
        text = wrap('check(total >= tokens, "not enough");\nr.net_amount -= tokens;',
                    sig='void system_contract::removerefund( name account, asset tokens )')
        rows = wc.check_c3('t.cpp', text, {'removerefund'}, {})
        self.assertEqual(classes(rows), ['C3'])

    def test_validated_in_action_is_silent(self):
        text = wrap('check(tokens.amount > 0, "tokens must be positive");\nr.net_amount -= tokens;',
                    sig='void system_contract::removerefund( name account, asset tokens )')
        self.assertEqual(wc.check_c3('t.cpp', text, {'removerefund'}, {}), [])

    def test_validated_in_helper_is_silent(self):                     # validate_proposal_fields
        text = wrap('validate_fields( committee, funding_goal );\n_p.emplace(a, [&](auto& p){ p.goal = funding_goal; });',
                    sig='void system_contract::regproposal( name a, name committee, const asset& funding_goal )')
        defs = {'validate_fields': ('const name& committee, const asset& goal',
                                    '{ check(goal.is_valid(), "invalid quantity"); }')}
        self.assertEqual(wc.check_c3('t.cpp', text, {'regproposal'}, defs), [])

    def test_helper_and_non_action_are_silent(self):
        text = wrap('r.x -= tokens;', sig='void system_contract::helper( asset tokens )')
        self.assertEqual(wc.check_c3('t.cpp', text, {'other'}, {}), [])


class C2Narrowing(unittest.TestCase):
    def test_wide_product_into_uint32_fires(self):                    # WCAP-SYS-2026-010
        rows = wc.check_c2_narrowing('t.cpp', 'uint32_t secs = proposal.duration * seconds_per_day;\n', {'duration'})
        self.assertEqual(classes(rows), ['C2'])

    def test_narrow_operands_are_silent(self):
        rows = wc.check_c2_narrowing('t.cpp', 'uint32_t secs = env.duration_of_voting * seconds_per_day;\n', {'other'})
        self.assertEqual(rows, [])


if __name__ == '__main__':
    unittest.main(verbosity=1)

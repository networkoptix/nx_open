import logging

import pytest

log = logging.getLogger(__name__)


@pytest.fixture
def server(server_factory):
    return server_factory('server')


# storing only those event fields which should not change
class Rule(object):

    @classmethod
    def from_dict(cls, d):
        return cls(
            id=d['id'],
            event_type=d['eventType'],
            action_type=d['actionType'],
            )

    def __init__(self, id, event_type, action_type):
        self.id = id
        self.event_type = event_type
        self.action_type = action_type

    def __repr__(self):
        return '<%s: event_type=%s action_type=%s>' % (self.id, self.event_type, self.action_type)

    def __eq__(self, other):
        assert isinstance(other, Rule), repr(other)
        return (self.id == other.id and
                self.event_type == other.event_type and
                self.action_type == other.action_type)


# https://networkoptix.atlassian.net/browse/UT-46
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/85204446/Cloud+test
def test_events_reset(server):
    rule_dict_list = server.rest_api.ec2.getEventRules.GET()
    log.info('Initially server has %d event rules:', len(rule_dict_list))
    initial_rule_list = {
        rule.id: rule for rule in map(Rule.from_dict, rule_dict_list)}
    for rule in initial_rule_list:
        log.info('\t%s' % rule)
    log.info('Resetting rules...')
    server.rest_api.ec2.resetEventRules.POST(json={})
    rule_dict_list = server.rest_api.ec2.getEventRules.GET()
    log.info('After reset server has %d event rules:', len(rule_dict_list))
    final_rule_list = {
        rule.id: rule for rule in map(Rule.from_dict, rule_dict_list)}
    for rule in final_rule_list:
        log.info('\t%s' % rule)
    assert sorted(initial_rule_list.keys()) == sorted(final_rule_list)
    for id, initial_rule in initial_rule_list.items():
        assert final_rule_list[id] == initial_rule

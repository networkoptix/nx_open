import json
import logging
import operator
import os
from collections import defaultdict, deque
from datetime import datetime
import re

import boto3
from dateutil.parser import parse
from jinja2 import Environment, FileSystemLoader, select_autoescape

MAX_LINES_BEFORE = 20
MAX_LINES_AFTER = 10

logging.basicConfig(level=logging.DEBUG)

log = logging.getLogger()
log.setLevel(logging.INFO)


def format_date(dt):
    return dt.isoformat(sep=' ', timespec='milliseconds')


def parse_datetime(date_str):
    dt = parse(date_str)
    return int(1000 * dt.timestamp())


def render_template(template_file, context):
    root_path = os.path.abspath(os.path.dirname(__file__))
    env = Environment(
        loader=FileSystemLoader(os.path.join(root_path, 'templates')),
        autoescape=select_autoescape(['html', 'xml'])
    )

    env.filters['reformat_timestamp'] = lambda date_str: format_date(parse(date_str))

    template = env.get_template(template_file)
    return template.render(**context)


def generate_email_content(events, alarm_info, message):
    log.debug('Events are: {}'.format(events))

    context = {
        'alarm_info': alarm_info,
        'message': message,
        'events': events
    }

    text = render_template('alarm_email_template.html.jinja2', context)
    subject = 'Alarm on {}. {} ({}).'.format(alarm_info['instance_name'], alarm_info['module_name'],
                                             alarm_info['alarm_name'])
    email_content = {
        'Destination': {
            'ToAddresses': alarm_info['subscribers']
        },
        'Message': {
            'Body': {
                'Html': {
                    'Data': text
                }
            },
            'Subject': {
                'Data': subject
            }
        },
        'Source': 'Cloud Alarm <service@networkoptix.com>'
    }

    return email_content


def get_log_events(cwl, parameters):
    parameters = parameters.copy()

    next_token = 'Invalid'
    events = []
    while next_token:
        if next_token != 'Invalid':
            parameters['nextToken'] = next_token

        log_event_data = cwl.filter_log_events(**parameters)
        log.debug(log_event_data)

        events += log_event_data['events']
        next_token = log_event_data['nextToken'] if 'nextToken' in log_event_data else None

    return events


def format_date_from_msecs(msecs):
    dt = datetime.utcfromtimestamp(msecs / 1000.)
    return dt.isoformat(sep=' ', timespec='milliseconds')


def is_pattern_matched(line, filter_pattern):
    if filter_pattern.startswith('"'):
        return filter_pattern.strip('"') in line
    elif filter_pattern.startswith('['):
        filter_components = [x.strip() for x in filter_pattern.strip('[]').split(',') if x.strip()]
        line_components = [x.strip() for x in line.split(' ') if x.strip()]

        operands_map = {
            '<': operator.lt,
            '<=': operator.le,
            '>': operator.gt,
            '>=': operator.ge,
            '=': operator.eq,
            '!=': operator.ne
        }

        for n, f in enumerate(filter_components):
            if n >= len(line_components):
                return False

            l = line_components[n]
            if '<' in f or '>' in 'f' or '=' in f:
                try:
                    l_value = int(l)
                except ValueError:
                    break

                m = re.match(r'(\w+)([><=!]+)(.*)', f)
                if m:
                    f_split = m.groups()

                    if len(f_split) == 3:
                        operand = f_split[1]
                        value = int(f_split[2])

                        if operand in operands_map:
                            return operands_map[operand](l_value, value)

        return False


def get_logs_and_send_email(cwl, ses, alarm_info, message, metric_filter_data):
    timestamp = parse_datetime(message['StateChangeTime'])
    offset = message['Trigger']['Period'] * message['Trigger']['EvaluationPeriods'] * 1000

    events_grouped = None
    if metric_filter_data['metricFilters']:
        metric_filter = metric_filter_data['metricFilters'][0]

        log_group = metric_filter['logGroupName']
        filter_pattern = metric_filter['filterPattern']

        parameters = {
            'logGroupName': log_group,
            'startTime': timestamp - 2 * offset,
            'endTime': timestamp
        }

        log.debug('Filter parameters: {}'.format(parameters))

        events = get_log_events(cwl, parameters)

        # Deque of events + Matched flag pairs
        events_by_stream = defaultdict(lambda: deque(maxlen=MAX_LINES_BEFORE))
        matched_events_by_stream = defaultdict(list)

        for event in events:
            log_stream = event['logStreamName']
            ebs = events_by_stream[log_stream]

            text = event['message']
            matched = is_pattern_matched(text, filter_pattern)

            ts = format_date_from_msecs(event['timestamp'])
            ebs.append({'timestamp': ts,
                        'matched': matched,
                        'text': text})

            if matched:
                matched_events_by_stream[log_stream] += ebs

        alarm_info['log_group_name'] = log_group
        alarm_info['filter_pattern'] = filter_pattern

        events_grouped = [{'logStreamName': stream, 'filterPattern': filter_pattern, 'messages': messages} for
                          stream, messages in matched_events_by_stream.items()]
        log.debug('Events Grouped: {}'.format(events_grouped))

    log.info('===SENDING EMAIL===')

    email = generate_email_content(events_grouped, alarm_info, message)
    ses.send_email(**email)


def get_alarm_info(alarm_name, s3):
    response = s3.get_object(Bucket='nxcloud-instance-registry', Key='alarm_subscriptions.json')
    alarm_subscriptions = json.load(response['Body'])
    log.debug('Alarm subscriptions: {}'.format(alarm_subscriptions))

    alarm_info = None

    alarm_name_components = alarm_name.split('__')
    if len(alarm_name_components) == 3:
        log.debug('Alarm name components: {}'.format(alarm_name_components))
        alarm_info = next(
            filter(lambda x: alarm_name_components == [x['instance_name'], x['module_name'], x['alarm_name']],
                   alarm_subscriptions['alarms']), None)
        log.debug('Alarm info: {}'.format(alarm_info))

    return alarm_info


def lambda_handler(event, context):
    log.info('Starting handler')

    s3 = boto3.client('s3')
    cwl = boto3.client('logs')
    ses = boto3.client('ses')

    message = json.loads(event['Records'][0]['Sns']['Message'])

    log_file_name = '{}.json'.format(message['StateChangeTime'])
    s3.put_object(Bucket='log-dispatcher-logs', Key=log_file_name, Body=json.dumps(event))

    log.info('Saved source event to {}'.format(log_file_name))

    alarm_name = message['AlarmName']
    alarm_info = get_alarm_info(alarm_name, s3)

    request_params = {
        'metricName': message['Trigger']['MetricName'],
        'metricNamespace': message['Trigger']['Namespace']
    }

    metric_filter_data = cwl.describe_metric_filters(**request_params)
    log.debug('Metric Filter data is: {}'.format(metric_filter_data))
    get_logs_and_send_email(cwl, ses, alarm_info, message, metric_filter_data)


if __name__ == '__main__':
    event = json.load(open('event.json'))
    message = json.loads(event['Records'][0]['Sns']['Message'])

    context = {
        'Records': [
            {
                'Sns': {
                    'Message': message
                }
            }
        ]
    }

    lambda_handler(event, context)

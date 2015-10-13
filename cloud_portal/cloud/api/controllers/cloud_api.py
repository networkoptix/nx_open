__author__ = 'noptix'

from requests.auth import HTTPDigestAuth
from hashlib import md5

class account(object):
    def register(email,password,firstName, lastName):
        '''
            <function>
                <name>register</name>
                <description></description>
                <method>GET</method>
                <params>
                    <param>
                        <name>email</name>
                        <description>Unique address to be associated with account</description>
                        <optional>false</optional>
                    </param>
                    <param>
                        <name>passwordHa1</name>
                        <description>Hex representation of HA1 (see rfc2617) digest of user's password. Realm is VMS</description>
                        <optional>true</optional>
                    </param>
                    <param>
                        <name>fullName</name>
                        <description></description>
                        <optional>true</optional>
                    </param>
                    <param>
                        <name>customization</name>
                        <description>Customization of portal has been used to create account</description>
                        <optional>true</optional>
                    </param>
                </params>
                <result>
                    <caption>Account activation code in JSON format</caption>
                </result>
            </function>
        '''


        customization = CLOUD_CONNECT['customization']
        realm = CLOUD_CONNECT['password_realm']
        api_url = CLOUD_CONNECT['url']

        passwordString = '%:%:%'%(username,realm,password)
        passwordHA1 = md5(passwordString).hexdigest()
        fullName = '% %'%(firstName,lastName)

        #
        request = '%/register?email=%&passwordHA1=%&fullName=%&customization=%' % (api_url,email,passwordHA1,fullName,customization)
        response = requests.get(request)
        if response.status_code != 200:
            return None

        code = response.json()
        # REMOVE quotes here, or unJSON
        return code

    def activate(code):
        '''
            <function>
                <name>activate</name>
                <description></description>
                <method>GET</method>
                <params>
                    <param>
                        <name>code</name>
                        <description>Activation code provided by register method</description>
                        <optional>false</optional>
                    </param>
                </params>
                <result>
                    <caption>Account data JSON</caption>
                </result>
            </function>
        '''
        return None

    def get(code):


        # url = 'http://httpbin.org/digest-auth/auth/user/pass'

        # TODO: create wrappers
        result = requests.get(CLOUD_CONNECT['url'], auth=HTTPDigestAuth(email, password))

        '''
            <function>
                <name>get</name>
                <description>Get account information. Server determines account to return by provided credentials (email/password)</description>
                <method>GET</method>
                <result>
                    <caption>Account data JSON</caption>
                </result>
            </function>
        '''

        return None;
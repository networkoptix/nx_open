'''
Usage: signtool <command> [options]

        Valid commands:
                sign       --  Sign files using an embedded signature.
                timestamp  --  Timestamp previously-signed files.
                verify     --  Verify embedded or catalog signatures.
                catdb      --  Modify a catalog database.

Usage: signtool sign [options] <filename(s)>

Use the "sign" command to sign files using embedded signatures. Signing
protects a file from tampering, and allows users to verify the signer (you)
based on a signing certificate. The options below allow you to specify signing
parameters and to select the signing certificate you wish to use.

Certificate selection options:
/a          Select the best signing cert automatically. SignTool will find all
            valid certs that satisfy all specified conditions and select the
            one that is valid for the longest. If this option is not present,
            SignTool will expect to find only one valid signing cert.
/ac <file>  Add an additional certificate, from <file>, to the signature block.
/c <name>   Specify the Certificate Template Name (Microsoft extension) of the
            signing cert.
/f <file>   Specify the signing cert in a file. If this file is a PFX with
            a password, the password may be supplied with the "/p" option.
            If the file does not contain private keys, use the "/csp" and "/kc"
            options to specify the CSP and container name of the private key.
/i <name>   Specify the Issuer of the signing cert, or a substring.
/n <name>   Specify the Subject Name of the signing cert, or a substring.
/p <pass.>  Specify a password to use when opening the PFX file.
/r <name>   Specify the Subject Name of a Root cert that the signing cert must
            chain to.
/s <name>   Specify the Store to open when searching for the cert. The default
            is the "MY" Store.
/sm         Open a Machine store instead of a User store.
/sha1 <h>   Specify the SHA1 hash of the signing cert.
/fd         Specifies the file digest algorithm to use for creating file
            signatures. (Default is SHA1)
/u <usage>  Specify the Enhanced Key Usage that must be present in the cert.
            The parameter may be specified by OID or by string. The default
            usage is "Code Signing" (1.3.6.1.5.5.7.3.3).
/uw         Specify usage of "Windows System Component Verification"
            (1.3.6.1.4.1.311.10.3.6).

Private Key selection options:
/csp <name> Specify the CSP containing the Private Key Container.
/kc <name>  Specify the Key Container Name of the Private Key.

Signing parameter options:
/d <desc.>  Provide a description of the signed content.
/du <URL>   Provide a URL with more information about the signed content.
/t <URL>    Specify the timestamp server's URL. If this option is not present,
            the signed file will not be timestamped. A warning is generated if
            timestamping fails.
/tr <URL>   Specifies the RFC 3161 timestamp server's URL. If this option
            (or /t) is not specified, the signed file will not be timestamped.
            A warning is generated if timestamping fails.  This switch cannot
            be used with the /t switch.
/td <alg>   Used with the /tr switch to request a digest algorithm used by the
            RFC 3161 timestamp server.

Other options:
/ph         Generate page hashes for executable files if supported.
/nph        Suppress page hashes for executable files if supported.
            The default is determined by the SIGNTOOL_PAGE_HASHES
            environment variable and by the wintrust.dll version.
/q          No output on success and minimal output on failure. As always,
            SignTool returns 0 on success, 1 on failure, and 2 on warning.
/v          Print verbose success and status messages. This may also provide
            slightly more information on error.
'''

import os
from environment import execute_command


def signtool_executable(signtool_directory):
    return os.path.join(signtool_directory, 'signtool.exe')


def common_signtool_options():
    return [
        '/td', 'sha256',
        '/fd', 'sha256',
        '/tr', 'http://tsa.startssl.com/rfc3161',
        '/v',
        '/a']


def sign_command(
    signtool_directory,
    target_file,
    sign_description=None,
    sign_password=None,
    main_certificate=None,
    additional_certificate=None
):
    command = [signtool_executable(signtool_directory), 'sign']
    command += common_signtool_options()
    if sign_description:
        command += ['/d', sign_description]
    if sign_password:
        command += ['/p', sign_password]
    if main_certificate:
        command += ['/f', main_certificate]
    if additional_certificate:
        command += ['/ac', additional_certificate]
    command += [target_file]
    return command


def sign(
    signtool_directory,
    target_file,
    sign_description=None,
    sign_password=None,
    main_certificate=None,
    additional_certificate=None
):
    execute_command(sign_command(
        signtool_directory=signtool_directory,
        target_file=target_file,
        sign_description=sign_description,
        sign_password=sign_password,
        main_certificate=main_certificate,
        additional_certificate=additional_certificate))

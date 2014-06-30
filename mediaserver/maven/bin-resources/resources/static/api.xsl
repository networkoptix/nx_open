<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:clitype="clitype"
                xmlns:fn="http://www.w3.org/2005/xpath-functions" xmlns:iso4217="http://www.xbrl.org/2003/iso4217"
                xmlns:ix="http://www.xbrl.org/2008/inlineXBRL" xmlns:java="java"
                xmlns:link="http://www.xbrl.org/2003/linkbase"
                xmlns:xbrldi="http://xbrl.org/2006/xbrldi" xmlns:xbrli="http://www.xbrl.org/2003/instance"
                xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xs="http://www.w3.org/2001/XMLSchema"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                exclude-result-prefixes="clitype fn iso4217 ix java link xbrldi xbrli xlink xs xsi">
    <xsl:output method="html" omit-xml-declaration="yes" indent="yes" encoding="UTF-8"/>
    <xsl:variable name="XML" select="/"/>

    <xsl:template match="/">
        <xsl:text disable-output-escaping='yes'>&lt;!DOCTYPE html></xsl:text>
        <html>
            <head>
                <meta charset="utf-8"/>
                <meta http-equiv="X-UA-Compatible" content="IE=edge"/>
                <title></title>
                <meta name="description" content=""/>
                <meta name="viewport" content="width=device-width"/>
                <!-- Place favicon.ico and apple-touch-icon.png in the root directory -->
                <link rel="stylesheet" href="styles/29403685.vendor.css"/>
                <!-- build:css({.tmp,app}) styles/main.css -->
                <link rel="stylesheet" href="styles/2ffdb355.main.css"/>
            </head>
            <body>
              <div class="container">
                <nav class="navbar navbar-default navbar-fixed-top" role="navigation">
                  <div class="container">
                    <div class="navbar-header">
                      <h1>HD Witness
                      <small>API Reference</small>
                      </h1>
                    </div>
                  </div>
                </nav>
                
                    <div class="row">
                        <nav class="col-xs-4 bs-docs-sidebar">
                            <ul class="nav nav-stacked fixed" id="sidebar">
                                <xsl:for-each select="apidoc/groups/group">
                                    <xsl:variable name="groupName" select="groupName"/>
                                    <li>
                                        <a>
                                            <xsl:attribute name="href">#group_<xsl:value-of select="$groupName"/>
                                            </xsl:attribute>
                                            <xsl:value-of select="groupName"/>
                                        </a>
                                        <ul class="nav nav-stacked">
                                            <xsl:for-each select="functions/function">
                                                <xsl:variable name="quotedName" select="translate(name, '/', '_')"/>
                                                <li>
                                                    <a href="#execAction">
                                                        <xsl:attribute name="href">#group_<xsl:value-of select="$groupName"/>_method_<xsl:value-of select="$quotedName"/></xsl:attribute>
                                                        <xsl:value-of select="name"/>
                                                    </a>
                                                </li>
                                            </xsl:for-each>
                                        </ul>
                                    </li>
                                </xsl:for-each>
                            </ul>
                        </nav>

                        <div class="col-xs-8">
                            <xsl:for-each select="apidoc/groups/group">
                                <xsl:variable name="groupName" select="groupName"/>
                                <section>
                                    <xsl:attribute name="id">group_<xsl:value-of select="$groupName"/></xsl:attribute>

                                    <div class="page-header">
                                        <h3>
                                            <xsl:value-of select="groupDescription"/>
                                        </h3>
                                    </div>

                                    <xsl:for-each select="functions/function">
                                        <xsl:variable name="quotedName" select="translate(name, '/', '_')"/>
                                        <div class="subgroup">
                                            <xsl:attribute name="id">group_<xsl:value-of select="$groupName"/>_method_<xsl:value-of select="$quotedName"/></xsl:attribute>
                                            <h4>
                                                <span class="label label-info"><xsl:value-of select="method"/></span>
                                                <span class="label label-default">
                                                    /<xsl:value-of select="$groupName"/>/<xsl:value-of select="name"/>
                                                </span>
                                            </h4>
                                            <!--<h4><span class="label label-default">GET</span> /api/execAction</h4>-->

                                            <div class="well">
                                                <xsl:value-of select="description"/>
                                            </div>

                                            <dl>
                                                <dt>Parameters</dt>
                                                <dd>
                                                    <table class="table table-bordered table-condensed">
                                                        <tr>
                                                            <th>Name</th>
                                                            <th>Description</th>
                                                            <th>Optional</th>
                                                        </tr>
                                                        <xsl:for-each select="params/param">
                                                            <tr>
                                                                <td>
                                                                    <xsl:value-of select="name"/>
                                                                </td>
                                                                <td>
                                                                    <xsl:value-of select="description"/>
                                                                </td>
                                                                <td>
                                                                    <xsl:value-of select="optional"/>
                                                                </td>
                                                            </tr>
                                                        </xsl:for-each>
                                                    </table>
                                                </dd>
                                                <dt>Result</dt>
                                                <dd>
                                                    <xsl:value-of select="result"/>
                                                </dd>
                                            </dl>
                                        </div>

                                        <hr/>
                                    </xsl:for-each>

                                </section>

                            </xsl:for-each>
                        </div>
                    </div>


                </div>
            </body>

            <script src="bower_components/jquery/jquery.js"></script>
            <script src="bower_components/sass-bootstrap/dist/js/bootstrap.js"></script>
            <script src="bower_components/jquery-scrollspy-thesmart/scrollspy.js"></script>

            <script>
                $('body').scrollspy({
                target: '.bs-docs-sidebar',
                offset: 40
                });
            </script>
        </html>
    </xsl:template>
</xsl:stylesheet>

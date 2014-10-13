<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:clitype="clitype" xmlns:fn="http://www.w3.org/2005/xpath-functions"
    xmlns:iso4217="http://www.xbrl.org/2003/iso4217" xmlns:ix="http://www.xbrl.org/2008/inlineXBRL"
    xmlns:java="java" xmlns:link="http://www.xbrl.org/2003/linkbase"
    xmlns:xbrldi="http://xbrl.org/2006/xbrldi" xmlns:xbrli="http://www.xbrl.org/2003/instance"
    xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    exclude-result-prefixes="clitype fn iso4217 ix java link xbrldi xbrli xlink xs xsi">
    <xsl:output method="html" omit-xml-declaration="yes" indent="yes" encoding="UTF-8"/>
    <xsl:variable name="XML" select="/"/>
    <xsl:template match="/">
        <xsl:text disable-output-escaping="yes">&lt;!DOCTYPE html></xsl:text>
        <html>
            <head>
                <meta charset="utf-8"/>
                <meta http-equiv="X-UA-Compatible" content="IE=edge"/>
                <title/>
                <meta name="description" content=""/>
                <meta name="viewport" content="width=device-width"/>
                <!-- Place favicon.ico and apple-touch-icon.png in the root directory -->
                <link rel="stylesheet" href="styles/29403685.vendor.css"/>

                <link rel="stylesheet" href="styles/3889fc7d.main.css"/>
            </head>
            <body>
                <div class="container">
                    <nav class="navbar navbar-default navbar-fixed-top" role="navigation">
                        <div class="container">
                            <div class="navbar-header">
                                <h1>HD Witness <small>API Reference</small>
                                </h1>
                            </div>
                        </div>
                    </nav> <div class="row">
                        <nav class="col-xs-4 bs-docs-sidebar">
                            <ul class="nav nav-stacked fixed" id="sidebar">
                                <xsl:for-each select="/apidoc/groups/group">
                                    <xsl:variable name="groupName"
                                        select="translate(groupName, ' ', '_')"/>
                                    <xsl:variable name="urlPrefix" select="urlPrefix"/>
                                    <li>
                                        <a>
                                            <xsl:attribute name="href">#group_<xsl:value-of
                                                  select="$groupName"/>
                                            </xsl:attribute>
                                            <xsl:value-of select="groupName"/>
                                        </a>
                                        <ul class="nav nav-stacked">
                                            <xsl:for-each select="functions/function">
                                                <xsl:variable name="quotedName"
                                                  select="translate(name, '/&lt;&gt;', '___')"/>
                                                <li>
                                                  <a href="#execAction">
                                                  <xsl:attribute name="href">#group_<xsl:value-of
                                                  select="$groupName"/>_method_<xsl:value-of
                                                  select="$quotedName"/></xsl:attribute>
                                                  <xsl:value-of select="name"/>
                                                  </a>
                                                </li>
                                            </xsl:for-each>
                                        </ul>
                                    </li>
                                </xsl:for-each>
                            </ul>
                        </nav> <div class="col-xs-8">
                            <xsl:for-each select="apidoc/groups/group">
                                <xsl:variable name="groupName"
                                    select="translate(groupName, ' ', '_')"/>
                                <xsl:variable name="urlPrefix" select="urlPrefix"/>
                                <section style="padding-top: 40px; margin-top: -40px;">
                                    <xsl:attribute name="id">group_<xsl:value-of select="$groupName"
                                        /></xsl:attribute> <div class="page-header">
                                        <h3>
                                            <xsl:value-of select="groupDescription"/>
                                        </h3>
                                    </div> <xsl:for-each select="functions/function">
                                        <xsl:variable name="quotedName"
                                            select="translate(name, '/&lt;&gt;', '___')"/>
                                        <div class="subgroup"
                                            style="padding-top: 70px; margin-top: -70px;">
                                            <xsl:attribute name="id">group_<xsl:value-of
                                                  select="$groupName"/>_method_<xsl:value-of
                                                  select="$quotedName"/></xsl:attribute>
                                            <h4>
                                                <span class="label label-info"><xsl:value-of
                                                  select="method"/></span>
                                                <span class="label label-default">
                                                  <xsl:value-of select="$urlPrefix"/>/<xsl:value-of
                                                  select="name"/>
                                                </span>
                                            </h4> <div class="well">
                                                <xsl:value-of select="description"/>
                                            </div> <dl>
                                                <dt>Parameters</dt>
                                                <dd>
                                                  <xsl:choose>
                                                  <xsl:when test="params/param">
                                                  <table
                                                  class="table table-bordered table-condensed">
                                                  <tr>
                                                  <th>Name</th>
                                                  <th>Description</th>
                                                  <th>Optional</th>
                                                  <xsl:if test="params/param/values/value">
                                                  <th>Values</th>
                                                  </xsl:if>
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
                                                  <xsl:if test="../../params/param/values/value">
                                                  <td>
                                                  <ul class="list-unstyled">
                                                  <xsl:for-each select="values/value">
                                                  <li>
                                                  <xsl:value-of select="name"/>
                                                  <a style="cursor: pointer;" data-toggle="tooltip"
                                                  data-placement="right"
                                                  data-trigger="hover focus click">
                                                  <xsl:attribute name="title">
                                                  <xsl:value-of select="description"/>
                                                  </xsl:attribute> (?) </a>
                                                  </li>
                                                  </xsl:for-each>
                                                  </ul>
                                                  </td>
                                                  </xsl:if>
                                                  </tr>
                                                  </xsl:for-each>
                                                  </table>
                                                  </xsl:when>
                                                  <xsl:otherwise> No parameters </xsl:otherwise>
                                                  </xsl:choose>
                                                </dd>
                                                <dt>Result</dt>
                                                <dd><xsl:apply-templates select="result"/></dd>
                                            </dl>
                                        </div> <hr/>
                                    </xsl:for-each> </section> </xsl:for-each>
                        </div>
                    </div> </div>
            </body>


            <script src="scripts/5ed9c348.api_documentation.js"></script>

            <script>
                $(function () { 
                    $("[data-toggle='tooltip']").tooltip(); 

                    $('body').scrollspy({
                        target: '.bs-docs-sidebar',
                        offset: 40
                    });
                });
            </script>
        </html>
    </xsl:template>
    <xsl:template match="result">
            <xsl:value-of select="caption"/>
        
        <xsl:if test="attributes/attribute">
            <div class="panel panel-default">
                    <table class="table">
                        <xsl:for-each select="attributes/attribute">
                            <tr>
                                <td>
                                    <xsl:value-of select="name"/>
                                </td>
                                <td>
                                    <xsl:value-of select="description"/>
                                </td>
                            </tr>
                        </xsl:for-each>
                    </table>
            </div>
        </xsl:if>
    </xsl:template>
</xsl:stylesheet>

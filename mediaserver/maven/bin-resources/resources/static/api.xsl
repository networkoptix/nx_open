<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:clitype="clitype" xmlns:fn="http://www.w3.org/2005/xpath-functions"
    xmlns:iso4217="http://www.xbrl.org/2003/iso4217" xmlns:ix="http://www.xbrl.org/2008/inlineXBRL"
    xmlns:java="java" xmlns:link="http://www.xbrl.org/2003/linkbase"
    xmlns:xbrldi="http://xbrl.org/2006/xbrldi" xmlns:xbrli="http://www.xbrl.org/2003/instance"
    xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    exclude-result-prefixes="clitype fn iso4217 ix java link xbrldi xbrli xlink xs xsi">
    <xsl:output
            method="html"
            doctype-system="about:legacy-compat"
            indent="yes"
            encoding="UTF-8"/>

    <xsl:variable name="XML" select="/"/>
    <xsl:template match="/">
        <html>
            <head>
                <meta charset="utf-8"/>
                <meta http-equiv="X-UA-Compatible" content="IE=edge"/>
                <title/>
                <meta name="description" content=""/>
                <meta name="viewport" content="width=device-width"/>
                <!-- Place favicon.ico and apple-touch-icon.png in the root directory -->
                <link rel="stylesheet" href="styles/409938f1.vendor.css">

                <link rel="stylesheet" href="styles/55d7854c.main.css">

                <link rel="stylesheet" href="customization/styles.css"/>
            </head>
            <body style="min-width: 450px;">
                <header class="navbar navbar-static-top bs-docs-nav" id="top" role="banner">
                    <div class="container">
                        <div class="navbar-header">
                            <a class="navbar-brand" data-toggle="tooltip" data-placement="bottom" href="index.html">
                                <img src="customization/webadmin_logo.png" height="48"/>
                            </a>
                            <h3>API reference</h3>
                        </div>
                        <nav class="collapse navbar-collapse bs-navbar-collapse" role="navigation">
                            <ul class="nav navbar-nav navbar-right">
                                <li ><a href="index.html">Web administration</a></li>
                            </ul>
                        </nav>
                    </div>
                </header>

                <div class="container">
                    <div class="row">
                        <nav class="col-sm-4 bs-docs-sidebar hidden-xs">
                            <ul class="nav nav-stacked fixed" id="sidebar">
                                <xsl:for-each select="/apidoc/groups/group">
                                    <xsl:variable name="groupName"
                                        select="translate(groupName, ' ()', '___')"/>
                                    <xsl:variable name="urlPrefix" select="urlPrefix"/>
                                    <li>
                                        <a>
                                            <xsl:attribute name="href">#group_<xsl:value-of
                                                  select="$groupName"/>
                                            </xsl:attribute>

                                            <span class="glyphicon"></span>
                                            <xsl:value-of select="groupName"/>
                                        </a>
                                        <ul class="nav nav-stacked">
                                            <xsl:for-each select="functions/function">
                                                <xsl:if test="not(@proprietary)">
                                                    <xsl:variable name="quotedName"
                                                      select="translate(name, '/&lt;&gt;', '___')"/>
                                                    <li>
                                                        <a href="#execAction">
                                                            <xsl:attribute name="href">#group_<xsl:value-of
                                                      select="$groupName"/>_method_<xsl:value-of
                                                      select="$quotedName"/></xsl:attribute>
                                                            <xsl:choose>
                                                                <xsl:when test="caption">
                                                                    <xsl:value-of select="caption"/>
                                                                </xsl:when>
                                                                <xsl:otherwise>
                                                                    <xsl:value-of select="name"/>
                                                                </xsl:otherwise>
                                                            </xsl:choose>
                                                        </a>
                                                    </li>
                                                </xsl:if>
                                            </xsl:for-each>
                                        </ul>
                                    </li>
                                </xsl:for-each>
                            </ul>
                        </nav> <div class="col-sm-8">
                            <xsl:for-each select="apidoc/groups/group">
                                <xsl:variable name="groupName"
                                    select="translate(groupName, ' ()', '___')"/>
                                <xsl:variable name="urlPrefix" select="urlPrefix"/>
                                <section style="padding-top: 40px; margin-top: -40px;">
                                    <xsl:attribute name="id">group_<xsl:value-of select="$groupName"
                                        /></xsl:attribute> <div class="page-header">
                                        <h3>
                                            <xsl:value-of select="groupDescription"/>
                                        </h3>
                                    </div> <xsl:for-each select="functions/function">
                                        <xsl:if test="not(@proprietary)">
                                            <xsl:variable name="quotedName"
                                                select="translate(name, '/&lt;&gt;', '___')"/>
                                            <div class="subgroup"
                                                style="padding-top: 70px; margin-top: -70px;">
                                                <xsl:attribute name="id">group_<xsl:value-of
                                                      select="$groupName"/>_method_<xsl:value-of
                                                      select="$quotedName"/></xsl:attribute>
                                                <xsl:if test="caption">
                                                    <h4><div><xsl:value-of select="caption"/><br/></div></h4>
                                                </xsl:if>
                                                <h4>
                                                    <span class="label label-info"><xsl:value-of
                                                      select="method"/></span>
                                                    <span class="label label-default">
                                                        <xsl:value-of select="$urlPrefix"/>/<xsl:value-of
                                                      select="name"/>
                                                    </span>

                                                    <xsl:if test="method='GET'">
                                                        <span class="label label-success">
                                                            <a target="_blank">
                                                                <xsl:attribute name="href"><xsl:value-of select="$urlPrefix"/>/<xsl:value-of select="name"/></xsl:attribute>
                                                                try method
                                                            </a>
                                                        </span>
                                                    </xsl:if>


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
                                        </xsl:if>
                                    </xsl:for-each> </section> </xsl:for-each>
                        </div>
                    </div> </div>
            </body>


            <script src="scripts/ad1872f1.api_documentation.js"></script>

            <script>
                $(function () {
                    //$("[data-toggle='tooltip']").tooltip();

                    $('body').scrollspy({
                        target: '.bs-docs-sidebar',
                        offset: 40
                    });

                    $(".nav .glyphicon").click(function(){
                        var $container = $(this).parent().parent();

                        // .active
                        // .pinned
                        // .pinned-hidden

                        if($container.hasClass("active")){
                            $container.removeClass("pinned")
                                    .toggleClass("pinned-hidden");
                        }else{
                            $container.toggleClass("pinned")
                                    .removeClass("pinned-hidden");
                        }
                        return false;
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

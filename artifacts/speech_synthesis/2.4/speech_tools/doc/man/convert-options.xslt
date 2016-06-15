<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
   <xsl:output method="text" indent="yes"/>

   <xsl:template match="/variablelist">
    <xsl:for-each select="varlistentry">
      <xsl:text>  - &lt;strong&gt;</xsl:text>
             <xsl:value-of select="term"/>
      <xsl:text>&lt;/strong&gt;: </xsl:text>
     <xsl:if test="LISTITEM/PARA/replaceable/text()">
         <xsl:text>*</xsl:text><xsl:value-of select="LISTITEM/PARA/replaceable/text()"/>
         <xsl:text>* </xsl:text>
      </xsl:if>
      <xsl:value-of select="normalize-space(LISTITEM/PARA/text()[normalize-space()])"/>
      <xsl:text>

</xsl:text>
    </xsl:for-each>
   </xsl:template>

</xsl:stylesheet>

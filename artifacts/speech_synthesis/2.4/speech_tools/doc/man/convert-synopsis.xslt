<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
   <xsl:output method="text" indent="yes"/>

   <xsl:template match="/cmdsynopsis">
<!--
      <xsl:text># </xsl:text>
      <xsl:value-of select="command" />
      <xsl:text>
## Synopsis
</xsl:text>
-->

      <xsl:value-of select="command" />
      <xsl:text> </xsl:text>
      <xsl:value-of select="/cmdsynopsis/text()" />
      <xsl:text> </xsl:text>

    <xsl:for-each select="arg">
      <xsl:text>[</xsl:text><xsl:value-of select="text()"/>
      <xsl:if test="replaceable/text()">
         <xsl:text> *</xsl:text>
         <xsl:value-of select="replaceable"/>
         <xsl:text>*</xsl:text>
      </xsl:if>
      <xsl:text>] </xsl:text>
    </xsl:for-each>
   <xsl:text>
</xsl:text>
   </xsl:template>

</xsl:stylesheet>

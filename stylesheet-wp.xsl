<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:wp="http://wordpress.org/export/1.2/"
                xmlns:content="http://purl.org/rss/1.0/modules/content/">

<xsl:template match="/rss/channel" mode="get-title">
    WP Extended RSS:
    <xsl:value-of select="title" />
</xsl:template>


<xsl:template match="wp:category|wp:tag" mode="show">
    <tr class="short">
        <td>
            [<xsl:value-of select="position()" />]
            <xsl:call-template name="start-tag">
                <xsl:with-param name="class" select="'short-name'" />
            </xsl:call-template>
        </td>
        <xsl:variable name="n"><xsl:value-of select="wp:category_nicename" /><xsl:value-of select="wp:tag_name" /></xsl:variable>
        <td><xsl:value-of select="wp:cat_name" /><xsl:value-of select="wp:tag_name" /></td>
        <td><xsl:value-of select="count(../item/category[@nicename = $n])" /></td>
    </tr>
    <tr class="long">
        <td colspan="3">
            <xsl:apply-templates select="." mode="inside"/>
        </td>
    </tr>
</xsl:template>


<xsl:template match="wp:category|wp:tag" mode="inside">
    <xsl:call-template name="element-block" />
</xsl:template>


<xsl:template match="wp:category[1]">
    <table>
        <tr>
            <th></th>
            <th>Name</th>
            <th>Item count</th>
        </tr>
        <xsl:apply-templates select="../wp:category|../wp:tag" mode="show" />
    </table>
</xsl:template>


<xsl:template match="wp:tag[1]">
<xsl:if test="count(../wp:category) = 0">
    <table>
        <tr>
            <th></th>
            <th>Name</th>
        </tr>
        <xsl:apply-templates select="../wp:category|../wp:tag" mode="show" />
    </table>
</xsl:if>
</xsl:template>


<xsl:template match="wp:category|wp:tag">
</xsl:template>


<xsl:template match="item" mode="show">
    <tr class="short">
        <td>
            [<xsl:value-of select="position()" />]
            <xsl:call-template name="start-tag">
                <xsl:with-param name="class" select="'short-name'" />
            </xsl:call-template>
        </td>
        <td><xsl:value-of select="wp:post_type" /></td>
        <td><xsl:value-of select="title" /></td>
        <!-- <td><xsl:value-of select="content:encoded" disable-output-escaping="yes" /></td> -->
    </tr>
    <tr class="long">
        <td colspan="4">
            <xsl:apply-templates select="." mode="inside"/>
        </td>
    </tr>
</xsl:template>


<xsl:template match="item" mode="inside">
    <xsl:call-template name="element-block" />
</xsl:template>


<xsl:template match="item[1]">
    <table>
        <tr>
            <th></th>
            <th>Type</th>
            <th>Title</th>
            <th>Content</th>
        </tr>
        <xsl:apply-templates select="../item" mode="show" />
    </table>
</xsl:template>


<xsl:template match="item">
</xsl:template>

</xsl:stylesheet>

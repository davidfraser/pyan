<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet version="1.0"
    xmlns="http://graphml.graphdrawing.org/xmlns"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:y="http://www.yworks.com/xml/graphml"
    xmlns:wp="http://wordpress.org/export/1.2/">

<xsl:output method="xml" indent="yes" />

<xsl:template match="/">
    <graphml>
        <key id="k0" for="node" yfiles.type="nodegraphics" />
        <key id="k1" for="node" attr.name="url" attr.type="string" />
        <graph id="G">
            <xsl:apply-templates select="//channel" />
            <xsl:apply-templates select="//wp:category" />
            <xsl:apply-templates select="//item[wp:status = 'publish' and (wp:post_type = 'post' or wp:post_type = 'page')]" />
        </graph>
    </graphml>
</xsl:template>

<xsl:template match="channel">
    <node id="{generate-id(.)}">
        <data key="k0">
            <y:ShapeNode>
                <y:NodeLabel><xsl:value-of select="title" /></y:NodeLabel>
                <y:Shape type="ellipse" />
            </y:ShapeNode>
        </data>
        <data key="k1"><xsl:value-of select="concat(link, '/')" /></data>
    </node>
</xsl:template>

<xsl:template match="wp:category">
    <node id="{generate-id(.)}">
        <data key="k0">
            <y:ShapeNode>
                <y:NodeLabel><xsl:value-of select="wp:cat_name" /></y:NodeLabel>
                <y:Shape type="ellipse" />
            </y:ShapeNode>
        </data>
        <data key="k1"><xsl:value-of select="concat(../link, '/category/', wp:category_nicename, '/')" /></data>
    </node>
    
    <xsl:choose>
        <xsl:when test="wp:category_parent != ''">
            <xsl:variable name="parent-name" select="string(wp:category_parent)" />
            <xsl:variable name="parent-cat" select="../wp:category[wp:category_nicename = $parent-name]" />
            <edge source="{generate-id($parent-cat)}" target="{generate-id(.)}" />
        </xsl:when>
        <xsl:otherwise>
            <edge source="{generate-id(..)}" target="{generate-id(.)}" />
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template match="item">
    <node id="{generate-id(.)}">
        <data key="k0">
            <y:ShapeNode>
                <y:NodeLabel><xsl:value-of select="title" /></y:NodeLabel>
                <xsl:choose>
                    <xsl:when test="wp:post_type = 'post'">
                        <y:Shape type="rectangle" />
                    </xsl:when>
                    <xsl:when test="wp:post_type = 'page'">
                        <y:Shape type="roundrectangle" />
                    </xsl:when>
                </xsl:choose>
                <xsl:if test="wp:post_password != ''">
                    <y:BorderStyle type="dashed" />
                </xsl:if>
            </y:ShapeNode>
        </data>
        <data key="k1"><xsl:value-of select="link" /></data>
    </node>
    
    <xsl:for-each select="wp:comment[wp:comment_type = 'pingback']">
        <xsl:variable name="src-url" select="./wp:comment_author_url" />
        <xsl:variable name="src-item" select="/rss/channel/item[link = $src-url]" />
        <xsl:if test="count($src-item) != 0">
            <edge source="{generate-id($src-item)}" target="{generate-id(..)}" />
        </xsl:if>
    </xsl:for-each>
    
    <!-- <xsl:for-each select="category[@domain='category']">
        <xsl:variable name="cat-name" select="@nicename" />
        <xsl:variable name="cat-item" select="/rss/channel/wp:category[wp:category_nicename = $cat-name]" />
        <xsl:if test="count($cat-item) != 0">
            <edge source="{generate-id($cat-item)}" target="{generate-id(..)}" />
        </xsl:if>
    </xsl:for-each> -->
    
    <xsl:if test="wp:post_type = 'page'">
        <edge source="{generate-id(..)}" target="{generate-id(.)}" />
    </xsl:if>
</xsl:template>

</xsl:stylesheet>

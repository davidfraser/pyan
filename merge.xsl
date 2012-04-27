<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">

<xsl:output method="xml" indent="no" />

<xsl:param name="other" />


<xsl:template match="/">
    <xsl:apply-templates select="/" mode="merge">
        <xsl:with-param name="other" select="document($other)/*" />
    </xsl:apply-templates>
</xsl:template>


<xsl:template match="*" mode="merge">
    <xsl:param name="other" select="." />
    <xsl:param name="state" />
    
    <xsl:element name="{name()}">
        <xsl:if test="boolean($state)">
            <xsl:attribute name="state--"><xsl:value-of select="$state" /></xsl:attribute>
        </xsl:if>
        
        <!-- Common attributes. -->    
        <xsl:for-each select="@*">
            <xsl:variable name="n" select="name()" />
            <xsl:variable name="v" select="string(.)" />
            <xsl:for-each select="$other/@*[(name() = $n) and (. = $v)]">
                <xsl:attribute name="{name()}"><xsl:value-of select="$v" /></xsl:attribute>
            </xsl:for-each>
        </xsl:for-each>
        
        <!-- Deleted attributes. -->
        <xsl:for-each select="@*">
            <xsl:variable name="n" select="name()" />
            <xsl:variable name="v" select="string(.)" />
            <xsl:if test="not($other/@*[(name() = $n) and (. = $v)])">
                <xsl:attribute name="deleted--{name()}"><xsl:value-of select="$v" /></xsl:attribute>
            </xsl:if>
        </xsl:for-each>
        
        <!-- New attributes. -->
        <xsl:variable name="this" select="." />
        <xsl:for-each select="$other/@*">
            <xsl:variable name="n" select="name()" />
            <xsl:variable name="v" select="string(.)" />
            <xsl:if test="not($this/@*[(name() = $n) and (. = $v)])">
                <xsl:attribute name="new--{name()}"><xsl:value-of select="$v" /></xsl:attribute>
            </xsl:if>
        </xsl:for-each>
        
        <xsl:variable name="a-content" select="node()|comment()" />
        
        <xsl:variable name="b-content" select="($other/node()|$other/comment())" />
        
        <xsl:variable name="content">
            <xsl:call-template name="merge-content">
                <xsl:with-param name="a" select="$a-content" />
                <xsl:with-param name="b" select="$b-content" />
            </xsl:call-template>
        </xsl:variable>
        
        <xsl:copy-of select="$content" />
    </xsl:element>

</xsl:template>


<xsl:template match="text()" mode="merge">
    <xsl:param name="other" select="." />
    <xsl:param name="state" />
    <xsl:choose>
        <xsl:when test="$state = 'deleted'">
                <text--deleted><xsl:copy /></text--deleted>
        </xsl:when>
        <xsl:when test="$state = 'new'">
                <text--new><xsl:copy /></text--new>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="normalize-space(.) = normalize-space($other)">
                <xsl:copy />
            </xsl:if>
            <xsl:if test="normalize-space(.) != normalize-space($other)">
                <text--deleted><xsl:copy /></text--deleted>
                <text--new><xsl:copy-of select="$other" /></text--new>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>


<xsl:template match="comment()" mode="merge">
    <xsl:param name="other" select="." />
    <xsl:param name="state" />
    <xsl:choose>
        <xsl:when test="$state = 'deleted'">
                <text--deleted><xsl:copy /></text--deleted>
        </xsl:when>
        <xsl:when test="$state = 'new'">
                <text--new><xsl:copy /></text--new>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="normalize-space(.) = normalize-space($other)">
                <xsl:copy />
            </xsl:if>
            <xsl:if test="normalize-space(.) != normalize-space($other)">
                <text--deleted><xsl:copy /></text--deleted>
                <text--new><xsl:copy-of select="$other" /></text--new>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>


<!-- Get the signature of a node as a string.
If it's an element, return it surrounded by element delimiters, with no attributes.
If it's a comment, return it surrounded by comment delimiters.
Otherwise just return text.
-->
<xsl:template match="node()" mode="sig">
    &lt;<xsl:value-of select="name()" />&gt;
</xsl:template>

<xsl:template match="comment()" mode="sig">
    &lt;!--<xsl:value-of select="string(.)" />--&gt;
</xsl:template>

<xsl:template match="text()" mode="sig">
    <xsl:value-of select="text()" />
</xsl:template>


<!-- Merge two nodesets a and b.  The result is:

    if one side is empty:
        emit the other side
    else if a[1]/name() = b[1]/name() then:
        merge a[1] and b[1]
        merge nodesets a[2-] and b[2-]
    else:
        amp = position of b[name()=a[1]/name()]
        bmp = position of a[name()=b[1]/name()]
        if bmp < amp then:
            emit a[1]
            merge a[2-] and b
        else:
            emit b[1]
            merge a and b[2-]
-->
<xsl:template name="merge-content">
    <xsl:param name="a" />
    <xsl:param name="b" />
    
    <xsl:choose>
        <xsl:when test="count($a) = 0">
            <xsl:apply-templates mode="merge" select="$b">
                <xsl:with-param name="state">new</xsl:with-param>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:when test="count($b) = 0">
            <xsl:apply-templates mode="merge" select="$a">
                <xsl:with-param name="state">deleted</xsl:with-param>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise>
            <xsl:variable name="a-sig">
                <xsl:apply-templates select="$a[1]" mode="sig"/>
            </xsl:variable>
            
            <xsl:variable name="b-sig">
                <xsl:apply-templates select="$b[1]" mode="sig"/>
            </xsl:variable>
            
            <xsl:choose>
                <xsl:when test="string($a-sig) = string($b-sig)">
                    <xsl:apply-templates mode="merge" select="$a[1]">
                        <xsl:with-param name="other" select="$b[1]" />
                    </xsl:apply-templates>
                    <xsl:call-template name="merge-content">
                        <xsl:with-param name="a" select="$a[position()>1]" />
                        <xsl:with-param name="b" select="$b[position()>1]" />
                    </xsl:call-template>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:variable name="amp">
                        <xsl:call-template name="match-position">
                            <xsl:with-param name="sig" select="$a-sig" />
                            <xsl:with-param name="list" select="$b" />
                        </xsl:call-template>
                    </xsl:variable>
                    
                    <xsl:variable name="bmp">
                        <xsl:call-template name="match-position">
                            <xsl:with-param name="sig" select="$b-sig" />
                            <xsl:with-param name="list" select="$a" />
                        </xsl:call-template>
                    </xsl:variable>
                    
                    <!-- (<xsl:value-of select="$a-sig" />:<xsl:value-of select="$amp" /> / <xsl:value-of select="$b-sig" />:<xsl:value-of select="$bmp" />) -->
                    
                    <xsl:choose>
                        <xsl:when test="number($bmp) &lt; number($amp)">
                            <xsl:apply-templates mode="merge" select="$a[position() &lt;= $bmp]">
                                <xsl:with-param name="state">deleted</xsl:with-param>
                            </xsl:apply-templates>
                            <xsl:call-template name="merge-content">
                                <xsl:with-param name="a" select="$a[position() &gt; $bmp]" />
                                <xsl:with-param name="b" select="$b" />
                            </xsl:call-template>
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:apply-templates mode="merge" select="$b[position() &lt;= $amp]">
                                <xsl:with-param name="state">new</xsl:with-param>
                            </xsl:apply-templates>
                            <xsl:call-template name="merge-content">
                                <xsl:with-param name="a" select="$a" />
                                <xsl:with-param name="b" select="$b[position() &gt; $amp]" />
                            </xsl:call-template>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:otherwise>
    </xsl:choose>

</xsl:template>


<xsl:template name="match-position">
    <xsl:param name="sig" />
    <xsl:param name="list" />
    
    <xsl:variable name="positions">
        <xsl:apply-templates select="$list" mode="position">
            <xsl:with-param name="sig" select="$sig" />
        </xsl:apply-templates>
    </xsl:variable>
    
    <xsl:variable name="first-pos" select="number(substring-before($positions, ','))" />
    
    <xsl:choose>
        <xsl:when test="$first-pos &gt; 0">
            <xsl:variable name="pos2">
                <xsl:for-each select="$list[1]">
                    <xsl:value-of select="position()" />
                </xsl:for-each>
            </xsl:variable>
            
            <xsl:value-of select="$first-pos - $pos2" />
        </xsl:when>
        <xsl:otherwise>
            999999
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>


<xsl:template match="*|text()|comment()" mode="position">
    <xsl:param name="sig" />
    
    <xsl:variable name="other-sig">
        <xsl:apply-templates select="." mode="sig" />
    </xsl:variable>
    
    <xsl:if test="string($sig) = string($other-sig)">
        <xsl:value-of select="position()" />,
    </xsl:if>
</xsl:template>


</xsl:stylesheet>

<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml">

<xsl:output method="xml" indent="no"
    doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" />


<xsl:param name="filename" />


<xsl:template match="/">
    <xsl:variable name="title">
        <xsl:value-of select="$filename" />: <xsl:apply-templates mode="get-title" />
    </xsl:variable>

    <html>
    <head>
         <title><xsl:value-of select="$title" /></title>
         <style type="text/css">
<xsl:comment>
table { border-collapse: collapse; border: 2px solid black; }
th, td { border: thin solid silver; padding: 2px 8px; }

.right { text-align: right; }

div.indent { padding-left: 2em; }

span.short-name, span.long-name, span.row-name { text-decoration: underline; }
span.short-name:hover { color: white; background: navy; }
span.long-name:hover { color: white; background: navy; }
span.row-name:hover { color: white; background: navy; }
tr.long { display: none; }

div.short { display: none; }
span.changed { background: khaki; }
.new > span.true { background: inherit; }
.deleted > span.true { background: inherit; }

span.value { font-family: monospace; color: black; }
span.id { font-weight: bold; font-family: monospace; color: black; }
span.idref { text-decoration: underline; font-family: monospace; color: black; }
span.tag { color: navy; }
span.attr { color: green; }
span.text { font-family: monospace; color: black; }

.new > div { background: inherit; }
.deleted > div { background: inherit; }

span.future { background: lime; }

span.comment { font-style: italic; color: grey; white-space: pre; }

ul { margin-top: 0; margin-bottom: 0; }

.deleted { background: #FFCCCC; }
.new { background: #CCFFCC; }
</xsl:comment>
         </style>
         <script type="text/javascript">
<xsl:comment>
function get_parent(e)
{
    e = e.parentNode.parentNode;
    while (e.tagName != 'TR' &amp;&amp; e.tagName != 'DIV')
    {
        e = e.parentNode;
    }
    
    if (e.parentNode.tagName == 'TR')
        e = e.parentNode;
    
    return e;
}


function show_element(e)
{
    if (e.tagName == 'TR')
        e.style.display = 'table-row';
    else
        e.style.display = 'block';
}


function click_short(e)
{
    p = get_parent(e.target);
    p.style.display = 'none';
    show_element(p.nextSibling);
}


function click_long(e)
{
    p = get_parent(e.target);
    if (p.parentNode.tagName == 'TD')
        p = get_parent(p);
    p.style.display = 'none';
    show_element(p.previousSibling);
}


function click_id(e)
{
    id = e.target.target_id;
    target = document.getElementById(id);
    while (target.parentNode) {
        if (target.style.display == 'none' &amp;&amp; target.className &amp;&amp; target.className.indexOf('long') != -1)
        {
            prev = target.previousSibling;
            prev.style.display = 'none';
            show_element(target);
        }
        target = target.parentNode;
    }
    return true;
}


function init()
{
    /* Hide short divs. */
    els = document.getElementsByTagName('div')
    for (e in els)
    {
        elt = els[e];
        
        if (elt.className &amp;&amp; elt.className.indexOf('short') != -1)
        {
            elt.style.display = 'none';
        }
    }
    
    /* Hide long rows. */
    els = document.getElementsByTagName('tr')
    for (e in els)
    {
        elt = els[e];
        
        if (elt.className &amp;&amp; elt.className.indexOf('long') != -1)
        {
            elt.style.display = 'none';
        }
    }
    
    els = document.getElementsByTagName('span')
    for (e in els)
    {
        elt = els[e];
        
        if (elt.className &amp;&amp; elt.className.indexOf('short-name') != -1)
        {
            elt.onclick = click_short;
        }
        
        if (elt.className &amp;&amp; elt.className.indexOf('long-name') != -1)
        {
            elt.onclick = click_long;
        }
    }
    
    els = document.getElementsByTagName('a')
    for (e in els)
    {
        elt = els[e];
        
        if (elt.href)
        {
            var target_id = elt.href.split('#')[1];
            if (target_id)
            {
                elt.target_id = target_id;
                elt.onclick = click_id;
            }
        }
    }
}

window.onload = init;
</xsl:comment>
        </script>
    </head>
    <body>
        <h1><xsl:value-of select="$title" /></h1>
        
        <xsl:apply-templates select="@*|node()|comment()" />
    </body>
    </html>
</xsl:template>


<!-- Schema-specific addons -->

<xsl:include href="stylesheet-wp.xsl" />


<xsl:template match="*" mode="get-title" priority="-0.5">
</xsl:template>


<!-- XML formatting -->

<xsl:template name="format-amount">
    <xsl:param name="value" />
    <xsl:if test="$value">
        <xsl:value-of select="format-number($value, '###,##0.00')" />
    </xsl:if>
</xsl:template>


<xsl:template name="element-block">
    <xsl:param name="content">
        <xsl:apply-templates select="node()|comment()" />
    </xsl:param>
    
    <xsl:variable name="state" select="./@state--" />
    
    <xsl:if test="./*">
        <div class="short {$state}">
            <xsl:call-template name="start-tag">
                <xsl:with-param name="class" select="'short-name'" />
            </xsl:call-template>
            ...
            <span class="tag">&lt;/<xsl:value-of select="name()" />&gt;</span>
        </div>
    </xsl:if>
    
    <xsl:if test="$content">
        <div class="long {$state}">
            <xsl:if test="@id">
                <a name="{@id}"></a>
            </xsl:if>
            
            <xsl:call-template name="start-tag">
            </xsl:call-template>
            
            <xsl:if test="./* or text() or comment()">
                <xsl:if test="normalize-space(text()) != ''">
                    <span class="value"><xsl:value-of select="text()" /></span>
                </xsl:if>
                
                <xsl:choose>
                    <xsl:when test="./* or comment()">
                        <div class="indent">
                            <xsl:copy-of select="$content" />
                        </div>
                    </xsl:when>
                </xsl:choose>
                
                <span class="tag">&lt;/<xsl:value-of select="name()" />&gt;</span>
            </xsl:if>
        </div>
    </xsl:if>
</xsl:template>


<xsl:template name="start-tag">
    <xsl:param name="class" select="'long-name'" />
    
    <xsl:variable name="status">
        <xsl:choose>
            <xsl:when test="@state-- = 'deleted'">
                deleted
            </xsl:when>
            <xsl:when test="@state-- = 'new'">
                new
            </xsl:when>
            <xsl:when test="count(@state--|.//*[@state-- != '' or name() = 'text--new' or name() = 'text--deleted']|.//@*[starts-with(name(), 'new--') or starts-with(name(), 'deleted--')]) != 0">
                changed
            </xsl:when>
        </xsl:choose>
    </xsl:variable>
    
    <span class="tag {$status}">&lt;<span>
        <xsl:if test="./*">
            <xsl:attribute name="class"><xsl:value-of select="$class" /></xsl:attribute>
        </xsl:if>
        
        <xsl:value-of select="name()" />
        </span>
        
        <xsl:for-each select="@*[name() != 'state--']">
            <xsl:variable name="state" select="substring-before(name(), '--')" />
            <xsl:variable name="name">
                <xsl:choose>
                    <xsl:when test="boolean(substring-after(name(), '--'))">
                        <xsl:value-of select="substring-after(name(), '--')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:value-of select="name()" />
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:variable>
            <xsl:text> </xsl:text>
            <span class="{$state}">
            <span class="attr">
                <xsl:value-of select="$name" />
            </span>=&quot;<span class="text">
                <xsl:if test="$name = 'id'">
                    <xsl:attribute name="class">id</xsl:attribute>
                </xsl:if>
                <xsl:choose>
                    <xsl:when test="$name = 'idref'">
                        <xsl:attribute name="class">idref</xsl:attribute>
                        <a href="#{.}"><xsl:value-of select="." /></a>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:value-of select="." />
                    </xsl:otherwise>
                </xsl:choose>
            </span>&quot;</span></xsl:for-each>
            
        <xsl:if test="not(./*) and not(text()) and not(comment())"> /</xsl:if>&gt;</span>
</xsl:template>


<xsl:template match="comment()">
    <div>
        <span class="text">&lt;!--</span> <span class="comment"><xsl:value-of select="." /></span> <span class="text">--&gt;</span>
    </div>
</xsl:template>


<xsl:template match="*[name() = 'text--deleted']">
    <span class="deleted">
        <xsl:apply-templates select="*|comment()|text()" />
    </span>
</xsl:template>

<xsl:template match="*[name() = 'text--new']">
    <span class="new">
        <xsl:apply-templates select="*|comment()|text()" />
    </span>
</xsl:template>

<xsl:template match="*">
    <xsl:call-template name="element-block" />
</xsl:template>


</xsl:stylesheet>

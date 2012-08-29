import sys
import psycopg2
from xml.sax.saxutils import escape
from optparse import OptionParser

def connect(**params):
    connstr = "dbname='%(dbname)s' user='%(user)s' host='%(host)s' password='%(password)s'" % params
    conn = psycopg2.connect(connstr)
    return conn

class Table(object):
    def __init__(self, schema, name):
        self.schema = schema
        self.name = name
        self.columns = []
        self.pk = None
        self.fks = []
    
    def add_column(self, name, type, nullable):
        self.columns.append((name, type, nullable))
    
    def set_pk(self, pk_columns):
        self.pk = pk_columns
    
    def add_fk(self, from_cols, to_schema, to_name, to_cols):
        self.fks.append((from_cols, to_schema, to_name, to_cols))
    
    def __repr__(self):
        col_list = ', '.join('%s %s' % (cn, ct) for cn, ct, cnn in self.columns)
        return '%s.%s(%s)' % (self.schema, self.name, col_list)

def get_tables(conn, schemas):
    schema_list = ", ".join("'%s'" % sn for sn in schemas)
    cursor = conn.cursor()
    sql = """SELECT table_schema, table_name FROM information_schema.tables WHERE table_schema IN (%s) AND table_type = 'BASE TABLE'""" % schema_list
    all_tables = []
    cursor.execute(sql)
    for table_schema, table_name in cursor.fetchall():
        t = Table(table_schema, table_name)
        
        sql = """SELECT column_name, COALESCE(domain_name, data_type) AS data_type, is_nullable FROM information_schema.columns
            WHERE table_schema = '%s' AND table_name = '%s' ORDER BY ordinal_position""" % (table_schema, table_name)
        cursor.execute(sql)
        for column_name, data_type, is_nullable in cursor.fetchall():
            t.add_column(column_name, data_type, (is_nullable == 'YES'))
        
        sql = """SELECT column_name FROM information_schema.constraint_column_usage
            WHERE constraint_name = (SELECT constraint_name FROM information_schema.table_constraints
                WHERE table_schema = '%s' AND table_name = '%s' AND constraint_type = 'PRIMARY KEY')""" % (table_schema, table_name)
        cursor.execute(sql)
        pk_cols = []
        for column_name, in cursor.fetchall():
            pk_cols.append(column_name)
        
        t.set_pk(pk_cols)
        
        sql = """SELECT constraint_name FROM information_schema.table_constraints
                WHERE table_schema = '%s' AND table_name = '%s' AND constraint_type = 'FOREIGN KEY'""" % (table_schema, table_name)
        cursor.execute(sql)
        for constraint_name, in cursor.fetchall():
            sql = """SELECT column_name FROM information_schema.key_column_usage
                WHERE constraint_name = '%s' ORDER BY position_in_unique_constraint""" % constraint_name
            cursor.execute(sql)
            from_cols = []
            for column_name in cursor.fetchall():
                from_cols.append(column_name)
                
            sql = """SELECT table_schema, table_name, column_name FROM information_schema.constraint_column_usage
                WHERE constraint_name = '%s'""" % constraint_name
            cursor.execute(sql)
            to_cols = []
            for table_schema, table_name, column_name in cursor.fetchall():
                to_schema = table_schema
                to_name = table_name
                to_cols.append(column_name)
            
            if to_name is None:
                continue
            
            t.add_fk(from_cols, to_schema, to_name, to_cols)
        
        all_tables.append(t)
    
    return all_tables

def to_html_table(table):
    parts = []
    parts.append('<table>\n')
    for name, type, nullable in table.columns:
        parts.append('<tr>')
        parts.append('<td>')
        if name in table.pk:
            parts.append('<u>%s</u>' % name)
        elif nullable:
            parts.append('<i>%s</i>' % name)
        else:
            parts.append(name)
        parts.append('</td>')
        parts.append('<td>%s</td>' % type)
        parts.append('</tr>\n')
    parts.append('</table>')
    return ''.join(parts)

def guess_node_height(table):
    ROW_HEIGHT = 22
    EXTRA = 36
    return len(table.columns) * ROW_HEIGHT + EXTRA

def to_graphml_node(table, node_id):
    label = "%s.%s" % (table.schema, table.name)
    content = escape('<html>' + to_html_table(table))
    height = guess_node_height(table)
    return """<node id="%(node_id)s">
      <data key="d6">
        <y:GenericNode configuration="com.yworks.entityRelationship.big_entity">
          <y:Geometry height="%(height)d" width="153.0" x="-76.5" y="226.5"/>
          <y:Fill color="#E8EEF7" color2="#B7C9E3" transparent="false"/>
          <y:BorderStyle color="#000000" type="line" width="1.0"/>
          <y:NodeLabel alignment="center" autoSizePolicy="content" backgroundColor="#B7C9E3"
            configuration="com.yworks.entityRelationship.label.name" fontFamily="Dialog"
            fontSize="12" fontStyle="plain" hasLineColor="false" height="18.701171875"
            modelName="internal" modelPosition="t" textColor="#000000" visible="true"
            width="45.349609375" x="53.8251953125" y="4.0">%(label)s</y:NodeLabel>
          <y:NodeLabel alignment="left" autoSizePolicy="content"
            configuration="com.yworks.entityRelationship.label.attributes" fontFamily="Dialog"
            fontSize="12" fontStyle="plain" hasBackgroundColor="false" hasLineColor="false"
            height="114.0" modelName="custom" textColor="#000000" visible="true" width="146.0"
            x="2.0" y="30.701171875">%(content)s<y:LabelModel>
              <y:ErdAttributesNodeLabelModel/>
            </y:LabelModel>
            <y:ModelParameter>
              <y:ErdAttributesNodeLabelModelParameter/>
            </y:ModelParameter>
          </y:NodeLabel>
          <y:StyleProperties>
            <y:Property class="java.lang.Boolean" name="y.view.ShadowNodePainter.SHADOW_PAINTING" value="true"/>
          </y:StyleProperties>
        </y:GenericNode>
      </data>
    </node>""" % locals()

def to_graphml_edge(from_id, to_id, edge_id):
    return """    <edge id="%s" source="%s" target="%s">
    </edge>""" % (edge_id, from_id, to_id)

def to_graphml(all_tables):
    next_id = 1
    for t in all_tables:
        t.node_id = next_id
        next_id += 1
    
    parts = []
    parts.append("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<graphml xmlns="http://graphml.graphdrawing.org/xmlns" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:y="http://www.yworks.com/xml/graphml" xmlns:yed="http://www.yworks.com/xml/yed/3"
    xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd">
  <key for="graphml" id="d0" yfiles.type="resources"/>
  <key for="port" id="d1" yfiles.type="portgraphics"/>
  <key for="port" id="d2" yfiles.type="portgeometry"/>
  <key for="port" id="d3" yfiles.type="portuserdata"/>
  <key attr.name="url" attr.type="string" for="node" id="d4"/>
  <key attr.name="description" attr.type="string" for="node" id="d5"/>
  <key for="node" id="d6" yfiles.type="nodegraphics"/>
  <key attr.name="Description" attr.type="string" for="graph" id="d7"/>
  <key attr.name="url" attr.type="string" for="edge" id="d8"/>
  <key attr.name="description" attr.type="string" for="edge" id="d9"/>
  <key for="edge" id="d10" yfiles.type="edgegraphics"/>
  <graph edgedefault="directed" id="G">
    <data key="d7"/>
""")

    for t in all_tables:
        parts.append(to_graphml_node(t, t.node_id))
    
    tables_by_name = {}
    for t in all_tables:
        tables_by_name[(t.schema, t.name)] = t
    
    for t in all_tables:
        for fk in t.fks:
            from_id = t.node_id
            to_id = tables_by_name[(fk[1], fk[2])].node_id
            parts.append(to_graphml_edge(from_id, to_id, next_id))
            next_id += 1
    
    parts.append("""  </graph>
  <data key="d0">
    <y:Resources/>
  </data>
</graphml>
""")
    return ''.join(parts)

def main():
    usage = """usage: %prog -h HOST -d DATBASE -u USER -p PASSWORD [-s SCHEMAS]"""
    desc = """Generate GraphML schema diagrams for PostgreSQL databases."""
    parser = OptionParser(usage=usage, description=desc, add_help_option=False)
    parser.add_option("-h", "--host", action="store", help="server host name")
    parser.add_option("-d", "--database", action="store", help="database name")
    parser.add_option("-u", "--user", action="store", help="database user to login as")
    parser.add_option("-p", "--password", action="store", help="user's password")
    parser.add_option("-s", "--schemas", action="store", default='public', help="list of schemas to process")
    
    options, args = parser.parse_args()
    
    if options.host is None:
        parser.error('host is required')
    if options.database is None:
        parser.error('database is required')
    if options.user is None:
        parser.error('user is required')
    if options.password is None:
        parser.error('password is required')
        
    params = {'user': options.user, 'password': options.password, 'dbname': options.database, 'host': options.host}
    schema_list = options.schemas.split(',')
    
    conn = connect(**params)
    all_tables = get_tables(conn, schema_list)
    conn.close()
    
    print to_graphml(all_tables)

if __name__ == '__main__':
    main()

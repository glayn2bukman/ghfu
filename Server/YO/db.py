"""
This file contains a simple API interface to the sqlite3 database infrastructure
"""
import sqlite3, os
import sys, __config__ as cfg

path = os.path.dirname(os.path.realpath(__file__))

def get_tables(dbfile): # db file is unopened...jus a file path...
    """
    return dict of format: {table_name:[(column, type),...]}
    """

    if not os.path.isfile(dbfile): return []

    cmd = "SELECT name FROM sqlite_master WHERE type='table'"

    db = sqlite3.connect(dbfile)
    cur = db.cursor()

    cur.execute(cmd)
    
    tables = {i[0]:[] for i in cur.fetchall()}
    
    for table in tables:
        cur.execute("pragma table_info({})".format(table))
        tables[table] = [col[1:3] for col in cur.fetchall()]
    
    db.close()
    
    return tables

class Database:
    def __init__(self, db_path, tables = {}, singular_columns = {}, identity_columns={}):
        '''
        db: database file
        tables: table:((field,type), ) eg {"my_table":(('age','int'), )}
        singular_columns: column thats supposed to have one unique value...eg admin for users!
                        format: {'column':COLUMN, 'value':UNIQUE_VALUE, 'index':COLUMN_INDEX_IN_FIELDS}
                        
                        eg {"my_table": {"column":1, "value":"admin", "index":1}}
                        
        identity_columns: column(in fields) with id equevalent values... eg uname for users
                        eg {"my_table":(0,)}
        '''
        
        if os.path.isfile(db_path):
            self.db = sqlite3.connect(db_path)
            self.cur = self.db.cursor()
            self.conn = True        

            self.tables = get_tables(db_path)
        else:
            self.conn = False
            self.tables = tables

        self.dbfile = db_path
        self.singular_columns, self.identity_columns = singular_columns, identity_columns
        
        if not self.singular_columns:
            for table in self.tables: self.singular_columns[table] = {}
        if not self.identity_columns:
            for table in self.tables: self.identity_columns[table] = tuple(range(len(self.tables[table])))
        
        if (not self.conn) and tables:
            self.tables = {}
            self.init_db(tables)
    
    def init_db(self, tables):
        for _table in tables:            
            self.add_table(_table, tables[_table], self.singular_columns[_table], self.identity_columns[_table])

    def close(self):
        if self.conn: self.db.close()
    
    def delete_table(self, table):
        if not self.conn: return 'no db connection found'
        
        self.cur.execute("drop table if exists {}".format(table))
        self.db.commit()
    
        try: 
            del(self.tables[table])
            del(self.identity_columns[table])
            del(self.singular_columns[table])
        except: pass

        return True

    def clear_table(self, table):
        if not self.conn: return 'no db connection found'

        try:
            self.cur.execute("delete from {}".format(table))
            self.db.commit()
        except:
            return 'table <{}> not found!'.format(table)

    
    def get_all(self, table=None, fields={}, strict=True):
        """
        if strict, look for exact match in DB, else look for {target in item}
        """
        # fields: {column/field: value} eg {'uname':'bukman'} for users...
        
        if not table:
            try: table = self.tables.keys()[0]
            except: return 'The system has no tables, please create some!'
        if not (table in self.tables): return 'The table <{}>is unknown!'.format(table)

        if not self.conn: return 'The system has no {} yet! Please create some'.format(table)
        
        if not fields: # fetch from all fields
            self.cur.execute('select * from {}'.format(table))
        else:
            if strict:
                values = tuple([fields[key] for key in fields])

                for column in self.identity_columns[table]: 
                    if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                        keys = ' and '.join([key+'=\"?\"' for key in fields]) 
                    else:
                        keys = ' and '.join([key+'=?' for key in fields]) 

                self.cur.execute('select * from {} where {}'.format(table, keys), values)
            else:
                values = tuple([fields[key] for key in fields])

                for column in self.identity_columns[table]: 
                    if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                        keys = ' and '.join([key+' like \"%?%\"' for key in fields]) 
                    else:
                        keys = ' and '.join([key+'=?' for key in fields]) 

                stmnt = 'select * from {} where {}'.format(table, keys)
                stmnt = stmnt.replace("?", "{}")
                self.cur.execute(stmnt.format(*values))
            
        fields = self.cur.fetchall()
        if fields==[]: return 'The system has no {} matching your search!'.format(table)
        return fields

    def add_table(self, table, fields, singular_column = {}, identity_column=()):

        if not self.conn: 
            self.db = sqlite3.connect(self.dbfile)
            self.cur = self.db.cursor()
            self.conn = True        

        if (table in self.tables) or (table in get_tables(self.dbfile)): 
            return "The system already has the table <{}>".format(table) 
        else:
            self.tables[table] = fields
            self.singular_columns[table] = singular_column
            if identity_column: self.identity_columns[table] = identity_column
            else: self.identity_columns[table] = tuple(range(len(self.tables[table])))
            
        _fields = ''
        for field in fields:
            _fields += ' '.join(field)+',\n'

        _fields = _fields.strip()[:-1]
    
        self.cur.execute('''create table {} (
                            {}
                        )'''.format(table, _fields))

        self.db.commit()

    def add(self, data, table=None):

        if not table:
            try: table = self.tables.keys()[0]
            except: return 'The system has no tables, please create some!'
        if not (table in self.tables): return 'The table <{}>is unknown!'.format(table)

        if self.singular_columns[table]:
            if data[self.singular_columns[table]['index']]==self.singular_columns[table]['value']: # in case we already have a sinnglular col value
                tag = (self.singular_columns[table]['value'],)
                self.cur.execute('select * from {} where rights=?'.format(table, tag))
                if self.cur.fetchone()!=None: return 'The system allows one occurance of type <{}>!'.format(tag[0])
        
        target = []
        for column in self.identity_columns[table]: 
            if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                target.append(str(data[column]))
            else:
                try: 
                    data[column]/2
                except:
                    return 'Column {}(index {}) should contain a Figure'.format(column+1, column)

                target.append(data[column])

            if data[column]==None: print target

        
        if self.is_user(tuple(target), table=table):
            return 'Entity <{}> is already in the system'.format(str(target))

        entries = ','.join(['?']*len(self.tables[table]))
        self.cur.execute('insert into {} values ({})'.format(table,entries), data)
        self.db.commit()

    def edit(self, target, new_data, table=None):
        if not table:
            try: table = self.tables.keys()[0]
            except: return 'The system has no tables, please create some!'
        if not (table in self.tables): return 'The table <{}>is unknown!'.format(table)

        if not self.conn: return 'Can\'t edit DB coz thez none!'

        if not self.is_user(target, table=table): 
            return 'Failed to edit <{}>, does not seem to exist in the system'.format(str(target))

        fields = ''
        for field in self.tables[table]:
            if field[1]=="text" or ("varchar" in field[1]):
                fields += field[0]+'=\"?\",\n'
            else:
                fields += field[0]+'=?,\n'
    
        fields = fields.strip()[:-1]

        _fields = []
        for column in self.identity_columns[table]: 
            if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                _fields.append(self.tables[table][column][0]+'=\"?\"')
            else:
                _fields.append(self.tables[table][column][0]+'=?')

        _fields = ' and '.join(_fields)

        stmnt = '''update {} set 
                                {}
                            where
                                {}
                                '''.format(table, fields, _fields)
        stmnt = stmnt.replace("?", "{}")

        self.cur.execute(stmnt.format(*(list(new_data)+list(target))))

        self.db.commit()
    
    def is_user(self, target, table=None):
        if not table:
            try: table = self.tables.keys()[0]
            except: return 'The system has no tables, please create some!'
        if not (table in self.tables): return 'The table <{}>is unknown!'.format(table)

        if not self.conn: return 'The system has no {} yet!'.format(table)

        if len(target)!=len(self.identity_columns[table]): 
            return 'tuple(targets) must contain all values in identity_columns!'

        fields = []
        for column in self.identity_columns[table]: 
            if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                fields.append(self.tables[table][column][0]+'=\"?\"')
            else:
                fields.append(self.tables[table][column][0]+'=?')
        fields = ' and '.join(fields)

        stmnt = 'select * from {} where {}'.format(table, fields)
        stmnt = stmnt.replace("?", "{}")

        self.cur.execute(stmnt.format(*target))

        if not self.cur.fetchone(): return False
        return True

    def delete(self, target, table=None): 
        if not table:
            try: table = self.tables.keys()[0]
            except: return 'The system has no tables, please create some!'
        if not (table in self.tables): return 'The table <{}>is unknown!'.format(table)
       
        if not self.is_user(target, table=table): 
            return 'Failed to delete <{}>, not in system!'.format(target)
        
        fields = []
        for column in self.identity_columns[table]: 
            if self.tables[table][column][1]=="text" or ("varchar" in self.tables[table][column][1]):
                fields.append(self.tables[table][column][0]+'=\"?\"')
            else:
                fields.append(self.tables[table][column][0]+'=?')

        fields = ' and '.join(fields)

        stmnt = 'delete from {} where {}'.format(table, fields)
        stmnt = stmnt.replace("?", "{}")
        self.cur.execute(stmnt.format(*target))
                    
        self.db.commit()
        #self.add(data)

dbfile = os.path.join(path, "db", "ghfu.config")
CONFIG_DB = None

def get_data(field):
    data = "Unknown field!"
    if field=="templates":
        data = {temp[len("yo_template_"):-5]:(CONFIG_DB.get_all(table=temp[:-5])[0][0], CONFIG_DB.get_all(table=temp)) for temp in CONFIG_DB.tables if "_vars" in temp}

    elif field=="methods":
        data = {mtd[0]: mtd[1] for mtd in CONFIG_DB.get_all(table="YO_methods")}

    elif field=="general":
        data = {}
        _data = CONFIG_DB.get_all(table="YO")[0]
        for index, column in enumerate(CONFIG_DB.tables["YO"]): data[column[0]] = _data[index]
    
    elif field=="urls":
        data = [url[0] for url in CONFIG_DB.get_all(table="YO_urls")]

    elif field=="currencies":
        data = {curr[0]:curr[1] for curr in CONFIG_DB.get_all(table="YO_currencies")}

    elif field=="headers":
        data = {header[0]:header[1] for header in CONFIG_DB.get_all(table="YO_headers")}

    elif field=="country_codes":
        data = {cc[0]: cc[1] for cc in CONFIG_DB.get_all(table="country_codes")}

    elif field=="data_bundles":
        data = {}
        for row in CONFIG_DB.get_all(table="data_bundles"):
            if not (row[0] in data): 
                data[row[0]]={}
            data[row[0]][row[1]] = row[2]

    elif field=="phonebook":
        data = {CONFIG_DB.tables["phonebook"][0][0]: CONFIG_DB.get_all(table="phonebook")[0][0]}

    
    return data

def create_db(overwrite=0):
    global CONFIG_DB        

    if os.path.isfile(dbfile):
        if not overwrite: 
            CONFIG_DB = Database(dbfile)            
            return
            
        while 1:
            choice = raw_input("Overwrite existing db file? (y/n) ").lower()
            if choice =="n": 
                print "Operation dumped..."
                return
            elif choice =="y": break

    CONFIG_DB = Database(dbfile)            

    print "\033c"

    if os.path.isfile(dbfile):
        print "deleing file...",
        os.remove(dbfile)
        print "done"
        
    with open(os.path.join(path, "__config__.py"), "r") as f:
        data = f.readlines()
        for index, line in enumerate(data):
            if "\"" in line:
                sys.exit("All strings in __config__.py must be of format \
\'string\' or \'\'\'string\'\'\' NOT \"string\" or \"\"\"string\"\"\".\
\nAt line {}, col {}, a \" was found".format(index+1, line.index("\"")+1)
                   )
            elif "None" in line:
                sys.exit("No value should be set to \'None\' in __config__.py\
. One has been found at line {} col {}".format(index+1, line.index("None")+1))

            elif "True" in line:
                sys.exit("No value should be set to \'True\' in __config__.py\
. One has been found at line {} col {}. Please set this value to 1 instead".format(index+1, line.index("True")+1))
            elif "False" in line:
                sys.exit("No value should be set to \'False\' in __config__.py\
. One has been found at line {} col {}. Please set this value to 0 instead".format(index+1, line.index("False")+1))
    
    

    tables = {
            
        "data_bundles": [(("duration","text"), ("bundle","text"), ("amount","float")),
            """
for _duration_ in cfg.DATA_BUNDLES:
    for data in [[k, cfg.DATA_BUNDLES[_duration_][k]] for k in cfg.DATA_BUNDLES[_duration_]]: 
        CONFIG_DB.add([_duration_]+data, table="data_bundles")
            """],
            
        "passwords":[(("pswd_type", "text"), ("pswd", "text")),
            """for data in [(k, cfg.PASSWORDS[k]) for k in cfg.PASSWORDS]: CONFIG_DB.add(data, table="passwords")"""],
            
        "phonebook":[(("file", "text"),),
            """CONFIG_DB.add([cfg.PHONEBOOK["file"]], table="phonebook")"""],
            
        "logs":[(("file", "text"), ("logall", "int")),
            """CONFIG_DB.add([cfg.LOGS["file"], cfg.LOGS["logall"]], table="logs")"""],
            
        "country_codes":[(("country", "text"), ("code", "text")),
            """for data in [(k, cfg.COUNTRY_CODES[k]) for k in cfg.COUNTRY_CODES]: CONFIG_DB.add(data, table="country_codes")"""],
            

        "YO":[(("uname","text"), ("pswd","text"), ("account_uname","text"), ("account_pswd","text")),
            """CONFIG_DB.add([cfg.YO["uname"], cfg.YO["pswd"], cfg.YO["account-uname"], cfg.YO["account-pswd"]], table="YO")"""],
            
        "YO_urls": [(("url","text"),),
            """for data in cfg.YO["urls"]: CONFIG_DB.add([data], table="YO_urls")"""],
            
        "YO_methods":[(("method", "text"), ("yo_method", "text")),
            """for data in [(k, cfg.YO["methods"][k]) for k in cfg.YO["methods"]]: CONFIG_DB.add(data, table="YO_methods")"""],
            
        "YO_currencies":[(("currency", "text"), ("yo_currency", "text")),
            """for data in [(k, cfg.YO["currencies"][k]) for k in cfg.YO["currencies"]]: CONFIG_DB.add(data, table="YO_currencies")"""],
            
        "YO_headers":[(("header", "text"), ("value", "text")),
            """for data in [(k, cfg.YO["headers"][k]) for k in cfg.YO["headers"]]: CONFIG_DB.add(data, table="YO_headers")"""],
        
        }

    # add YO templates...
    # YO templates have 2 tables; 
    #   YO_template_TEMPNAME: ("description", "text")
    #   YO_template_TEMPNAME_vars: ("var", "text"), ("required", "int")

    # NB: in reconstruction, add _tempplate to TEMPNAME eg <withdraw> -> <withdraw_template>

    for template in cfg.YO["templates"]:
        template_desc = "YO_template_"+template[:template.index("_template")]
        template_vars = "YO_template_"+template[:template.index("_template")]+"_vars"

        tables[template_desc] = [(("description", "text"),),
            """CONFIG_DB.add([cfg.YO["templates"]["{}"][0]], table="{}")""".format(template, template_desc)]

        tables[template_vars] = [(("var", "text"), ("required", "int")),
            """for data in cfg.YO["templates"]["{}"][1]: CONFIG_DB.add(data, table="{}")""".format(template, template_vars)]
    

    print "\033[1;33mcreating tables\033[0m..."
    i = 1
    for table in tables:
        print "{}) creating table \033[1;33m{}\033[0m...".format(i, table)
        CONFIG_DB.add_table(table, tables[table][0])
        print "    now populating table..."
        exec(tables[table][1])
        print
        i += 1

create_db(0)

if __name__=="__main__":
    create_db(1)

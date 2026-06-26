import re, os, sys, html

class LuabindHighVelocityV66:
    def __init__(self):
        self.signatures = {} 
        self.lua_api = {}    
        self.inheritance_map = {}
        # Only deep-scan these for overloads to save 90% of processing time
        self.priority_folders = ['Source', 'JuceLibraryCode', 'ctrlr'] 
        # Folders to ignore entirely to prevent "forever" hangs
        self.ignore_folders = ['boost', 'asiosdk', 'vstsdk', 'stk', 'libs']

    def is_junk_signature(self, args):
        clean = args.strip().lower()
        if not clean or clean == "()": return False
        if re.match(r'^\(?-?[0-9.]+\)?$', clean): return True
        if re.match(r'^\(?[a-z]\)?$', clean): return True
        return False

    def clean_args(self, args):
        if not args or args.strip().lower() in ["void", "nullptr"]: return "()"
        a = re.sub(r'=[^,)]+', '', args) 
        a = re.sub(r'juce::|std::|const|&|\*|override|final|virtual|inline|noexcept', '', a)
        clean = ' '.join(a.split()).strip().strip(',')
        return f"({html.escape(clean)})"

    def run(self, source_dir, output_xml):
        print(f"[*] Version 66: High-Velocity Scan starting...")
        
        # Phase 1: Targeted Signature Collection (Fast)
        for root, _, files in os.walk(source_dir):
            # Optimization: Only scan priority folders for signatures
            if not any(p in root for p in self.priority_folders): continue
            if any(i in root.lower() for i in self.ignore_folders): continue
            
            for file in files:
                if file.endswith(('.h', '.hpp')):
                    try:
                        with open(os.path.join(root, file), 'r', encoding='utf-8', errors='ignore') as f:
                            content = f.read()
                            # Inheritance tracking
                            for im in re.finditer(r'class\s+([A-Za-z0-9_]+)\s*:\s*(?:public|protected|private)\s+([A-Za-z0-9_:]+)', content):
                                child, parent = im.groups()
                                self.inheritance_map[child] = parent.split("::")[-1].strip()
                            # Signature collection
                            for sm in re.finditer(r'([A-Za-z0-9_]+)\s*\(([^;{]*)\)\s*(?:const|override|final|inline)?\s*[;{]', content):
                                name, args = sm.groups()
                                if name not in ["if", "while", "for", "switch", "return"] and not self.is_junk_signature(args):
                                    cleaned = self.clean_args(args)
                                    if name not in self.signatures: self.signatures[name] = []
                                    if cleaned not in self.signatures[name]: self.signatures[name].append(cleaned)
                    except: continue

        # Phase 2: Luabind Registration (Global but efficient)
        for root, _, files in os.walk(source_dir):
            if any(i in root.lower() for i in self.ignore_folders): continue
            for file in files:
                if file.endswith(('.cpp', '.i', '.h', '.hpp')):
                    try:
                        with open(os.path.join(root, file), 'r', encoding='utf-8', errors='ignore') as f:
                            content = f.read()
                            parts = re.split(r'class_\s*<', content)
                            for part in parts[1:]:
                                name_match = re.match(r'\s*([^,> ]+)[^>]*>\s*(?:\(\s*"([^"]+)"\s*\))?', part)
                                if not name_match: continue
                                cpp_name = name_match.group(1).split("::")[-1].strip()
                                lua_name = name_match.group(2) or cpp_name
                                if lua_name not in self.lua_api:
                                    self.lua_api[lua_name] = {"cpp": cpp_name, "methods": set(), "static": set(), "constructors": set()}
                                block = part[:part.find(';')] if ';' in part else part
                                if "constructor" in block: self.lua_api[lua_name]["constructors"].add(lua_name)
                                scope_match = re.search(r'\.scope\s*\[(.*?)\]', block, re.DOTALL)
                                if scope_match:
                                    for m in re.finditer(r'(?<!\.)def\s*\(\s*"([^"]+)"', scope_match.group(1)):
                                        if m.group(1) != lua_name: self.lua_api[lua_name]["static"].add(m.group(1))
                                for m in re.finditer(r'\.def\s*\(\s*"([^"]+)"', block):
                                    m_name = m.group(1)
                                    if m_name not in self.lua_api[lua_name]["static"] and m_name not in self.lua_api[lua_name]["constructors"]:
                                        self.lua_api[lua_name]["methods"].add(m_name)
                    except: continue
        self.generate_xml(output_xml)

    def generate_xml(self, output_file):
        xml = ['<?xml version="1.0" encoding="UTF-8" ?>', '<LuaAPI>']
        for name in sorted(self.lua_api.keys()):
            d = self.lua_api[name]
            cpp = d["cpp"]; parent = f' inherits="{self.inheritance_map[cpp]}"' if cpp in self.inheritance_map else ""
            xml.append(f'  <class name="{name}" cpp_name="{cpp}"{parent}>')
            def write_lines(names, m_type):
                for m in sorted(names):
                    for s in self.signatures.get(m, ["()"]):
                        xml.append(f'      <method name="{m}" args="{s}" type="{m_type}"/>')
            if d["methods"]:
                xml.append('    <methods>'); write_lines(d["methods"], "instance"); xml.append('    </methods>')
            if d["static"] or d["constructors"]:
                xml.append('    <static_methods>')
                write_lines(d["static"], "static")
                for m in sorted(d["constructors"]):
                    for s in self.signatures.get(cpp, ["()"]):
                        xml.append(f'      <method name="{m}" args="{s}" type="constructor"/>')
                xml.append('    </static_methods>')
            if d["constructors"]:
                xml.append('    <constructors>')
                for m in sorted(d["constructors"]):
                    for s in self.signatures.get(cpp, ["()"]):
                        xml.append(f'      <method name="{m}" args="{s}" />')
                xml.append('    </constructors>')
            xml.append('  </class>')
        xml.append('</LuaAPI>')
        with open(output_file, 'w', encoding='utf-8') as f: f.write('\n'.join(xml))
        print(f"[+] Version 66 complete.")

if __name__ == "__main__":
    LuabindHighVelocityV66().run(sys.argv[1], sys.argv[2])
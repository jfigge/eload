Import("env","projenv")

#print(env.Dump())
def get_build_flag_value(flag_name):
    build_flags = env.ParseFlags(env['BUILD_FLAGS'])
    flags_with_value_list = [build_flag for build_flag in build_flags.get('CPPDEFINES') if type(build_flag) == list]
    defines = {k: v for (k, v) in flags_with_value_list}
    return defines.get(flag_name)

def before_upload(source, target, env):
    color = get_build_flag_value("COOLTERM")
    if color:
        print("coolterm-cli " + get_build_flag_value("COOLTERM") + " -d -d -b " + get_build_flag_value("BAUD_RATE") + " -p " + env['UPLOAD_PORT'])
        env.Execute("coolterm-cli " + get_build_flag_value("COOLTERM") + " -d -d -b " + get_build_flag_value("BAUD_RATE") + " -p " + env['UPLOAD_PORT'])

def after_upload(source, target, env):
    color = get_build_flag_value("COOLTERM")
    if color:
        env.Execute("coolterm-cli " + color + " -c -c -r -r")

env.AddPreAction("upload", before_upload)

env.AddPostAction("upload", after_upload)
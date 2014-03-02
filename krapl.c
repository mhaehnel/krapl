#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>

#define failout(condition, str,error) if (condition) { printk(KERN_INFO str); return error; }

#define RAPL_POWER_UNIT 0x606

#define RAPL_PKG   0x610
#define RAPL_DRAM  0x618
#define RAPL_PP0   0x638
#define RAPL_PP1   0x640

#define RAPL_POWER_LIMIT    0x0 
#define RAPL_ENERGY_STATUS  0x1
#define RAPL_POLICY         0x2
#define RAPL_PERF_STATUS    0x3
#define RAPL_POWER_INFO     0x4

#define SYS_PKG  0
#define SYS_DRAM 1
#define SYS_PP0  2
#define SYS_PP1  3
#define SYS_ROOT 4

#define SANDY    0x2A
#define SANDY_EP 0x2D
#define IVY      0x3A
#define IVY_E    0x3E // or 3F?
#define HASWELL   0x3C
/*#define HASWELL_E 0x3F
#define HASWELL_45 0x45
#define HASWELL_46 0x45 ??*/

static struct bus_type rapl_subsys = {
    .name = "rapl",
    .dev_name = "rapl"
};

static struct kobject *krapl_kobject[MAX_NUMNODES][5];

typedef struct {
    unsigned int msr;
    unsigned long long *raw;
} msr_query;

static void __rdmsrl(void *arg) {
    msr_query *q = (msr_query*)arg;
    rdmsrl(q->msr,*(q->raw));
    return;
}

static int nodecpu_of_kobj(struct kobject *kobj) {
    int node,i;
    struct cpumask m = CPU_MASK_NONE;
    for_each_node(node) {
        for (i = 0; i < 5; i++) {
            if (krapl_kobject[node][i] == kobj) {
                m = *cpumask_of_node(node);
                break;
            }
        }
        if (!cpus_empty(m)) break;
    }
    if (cpus_empty(m))
        failout(cpus_empty(m),"No cpus on node!\n",-ENODEV);
    return first_cpu(m);
}

#define CREATE_REG_ATTR_RO(name,obj,domain,reg) \
    static ssize_t show_##name##_##domain(struct kobject *kobj, struct kobj_attribute *attr, char *buf) { \
        obj tmp; int cpu; \
        msr_query q = { .msr = reg, .raw = &(tmp.raw) }; \
        if ((cpu = nodecpu_of_kobj(kobj)) < 0) return cpu;\
        smp_call_function_single(cpu,__rdmsrl, (void*)&q,true); \
        return sprintf(buf,"%u\n",tmp.name); \
    }; \
    static struct kobj_attribute name##_##domain##_attribute = __ATTR(name,0444,show_##name##_##domain, NULL);

#define GET_ATTR(name,obj) &name##_##obj##_attribute.attr

typedef union {
    struct {
        unsigned power_unit:4;
        unsigned __dummy:4;
        unsigned energy_unit:5;
        unsigned __dummy2:3;
        unsigned time_unit:4;
    };
    unsigned long long raw;
} power_unit_struct;

typedef union {
    struct {
        unsigned limit_1:15;
        unsigned enable_limit_1:1;
        unsigned clamping_limit_1:1;
        unsigned time_window_1:7;
        unsigned _dummy:8;
        unsigned limit_2:15;
        unsigned enable_limit_2:1;
        unsigned clamping_limit_2:1;
        unsigned time_window_2:7;
        unsigned _dummy2:7;
        unsigned lock:1;
    };
    unsigned long long raw;
} pkg_power_limit_struct;

typedef union {
    struct {
        unsigned energy:32;
    };
    unsigned long long raw;
} pkg_energy_status_struct;

typedef union {
    struct {
        unsigned thermal_spec_power:15;
        unsigned _dummy:1;
        unsigned min_power:15;
        unsigned _dummy2:1;
        unsigned max_power:15;
        unsigned _dummy3:1;
        unsigned max_time_window:6;
    };
    unsigned long long raw;
} pkg_power_info_struct;

typedef union {
    struct {
        unsigned throttle_time:32;
    };
    unsigned long long raw;
} pkg_perf_status_struct;

typedef union {
    struct {
        unsigned limit:15;
        unsigned enable_limit:1;
        unsigned clamping_limit:1;
        unsigned time_window:7;
        unsigned _dummy:7;
        unsigned lock:1;
    };
    unsigned long long raw;
} pp_power_limit_struct;

typedef union {
    struct {
        unsigned priority:5;
    };
    unsigned long long raw;
} pp_policy_struct;

typedef pkg_energy_status_struct  pp_energy_status_struct;
typedef pkg_perf_status_struct    pp_perf_status_struct;

typedef pp_power_limit_struct     dram_power_limit_struct;
typedef pkg_energy_status_struct  dram_energy_status_struct;
typedef pkg_power_info_struct     dram_power_info_struct;
typedef pkg_perf_status_struct    dram_perf_status_struct;

#define MK_POWER_UNIT(name) CREATE_REG_ATTR_RO(name,power_unit_struct,units,RAPL_POWER_UNIT)
MK_POWER_UNIT(energy_unit)
MK_POWER_UNIT(time_unit)
MK_POWER_UNIT(power_unit)
#undef MK_POWER_UNIT

//PKG Power Limit
#define MK_POWER_LIMIT(name) CREATE_REG_ATTR_RO(name,pkg_power_limit_struct,pkg,RAPL_PKG+RAPL_POWER_LIMIT)
MK_POWER_LIMIT(limit_1);
MK_POWER_LIMIT(enable_limit_1);
MK_POWER_LIMIT(clamping_limit_1);
MK_POWER_LIMIT(time_window_1);
MK_POWER_LIMIT(limit_2);
MK_POWER_LIMIT(enable_limit_2);
MK_POWER_LIMIT(clamping_limit_2);
MK_POWER_LIMIT(time_window_2);
MK_POWER_LIMIT(lock);
#undef MK_POWER_LIMIT

//PKG Energy Status
CREATE_REG_ATTR_RO(energy,pkg_energy_status_struct,pkg,RAPL_PKG+RAPL_ENERGY_STATUS);

//PKG Power Info
#define MK_POWER_INFO(name) CREATE_REG_ATTR_RO(name,pkg_power_info_struct,pkg,RAPL_PKG+RAPL_POWER_INFO)
MK_POWER_INFO(thermal_spec_power);
MK_POWER_INFO(min_power);
MK_POWER_INFO(max_power);
MK_POWER_INFO(max_time_window);
#undef MK_POWER_INFO

//PKG Perf Status  (only on Servers!)
CREATE_REG_ATTR_RO(throttle_time,pkg_perf_status_struct,pkg,RAPL_PKG+RAPL_PERF_STATUS);

//PP0 Power Limit
#define MK_POWER_LIMIT(name) CREATE_REG_ATTR_RO(name,pp_power_limit_struct,pp0,RAPL_PP0+RAPL_POWER_LIMIT)
MK_POWER_LIMIT(limit);
MK_POWER_LIMIT(enable_limit);
MK_POWER_LIMIT(clamping_limit);
MK_POWER_LIMIT(time_window);
MK_POWER_LIMIT(lock);
#undef MK_POWER_LIMIT

//PP0 Energy Status
CREATE_REG_ATTR_RO(energy,pp_energy_status_struct,pp0,RAPL_PP0+RAPL_ENERGY_STATUS);

//PP0 Policy (only on non-servers)
CREATE_REG_ATTR_RO(priority,pp_policy_struct,pp0,RAPL_PP0+RAPL_POLICY);

//PP0 Perf Status (only on non-servers)
CREATE_REG_ATTR_RO(throttle_time,pp_perf_status_struct,pp0,RAPL_PP0+RAPL_PERF_STATUS);

//PP1 Power Limit (only on non-servers)
#define MK_POWER_LIMIT(name) CREATE_REG_ATTR_RO(name,pp_power_limit_struct,pp1,RAPL_PP1+RAPL_POWER_LIMIT)
MK_POWER_LIMIT(limit);
MK_POWER_LIMIT(enable_limit);
MK_POWER_LIMIT(clamping_limit);
MK_POWER_LIMIT(time_window);
MK_POWER_LIMIT(lock);
#undef MK_POWER_LIMIT

//PP1 Energy Status (only on non-servers)
CREATE_REG_ATTR_RO(energy,pp_energy_status_struct,pp1,RAPL_PP1+RAPL_ENERGY_STATUS);

//PP1 Policy (only on non-servers)
CREATE_REG_ATTR_RO(priority,pp_policy_struct,pp1,RAPL_PP1+RAPL_POLICY);

//DRAM Power Limit (only on servers)
#define MK_POWER_LIMIT(name) CREATE_REG_ATTR_RO(name,dram_power_limit_struct,dram,RAPL_DRAM+RAPL_POWER_LIMIT)
MK_POWER_LIMIT(limit);
MK_POWER_LIMIT(enable_limit);
MK_POWER_LIMIT(clamping_limit);
MK_POWER_LIMIT(time_window);
MK_POWER_LIMIT(lock);
#undef MK_POWER_LIMIT

//DRAM Energy Status (only on servers)
CREATE_REG_ATTR_RO(energy,dram_energy_status_struct,dram,RAPL_DRAM+RAPL_ENERGY_STATUS);

//DRAM Perf Status (only on servers)
CREATE_REG_ATTR_RO(throttle_time,dram_perf_status_struct,dram,RAPL_DRAM+RAPL_PERF_STATUS);

//DRAM Power Info (only on servers)
#define MK_POWER_INFO(name) CREATE_REG_ATTR_RO(name,dram_power_info_struct,dram,RAPL_DRAM+RAPL_POWER_INFO)
MK_POWER_INFO(thermal_spec_power);
MK_POWER_INFO(min_power);
MK_POWER_INFO(max_power);
MK_POWER_INFO(max_time_window);
#undef MK_POWER_INFO

static struct attribute *pkg_attrs[] = {
    GET_ATTR(limit_1,pkg),             //PKG Power Limit
    GET_ATTR(enable_limit_1,pkg),
    GET_ATTR(clamping_limit_1,pkg),
    GET_ATTR(time_window_1,pkg),
    GET_ATTR(limit_2,pkg),
    GET_ATTR(enable_limit_2,pkg),
    GET_ATTR(clamping_limit_2,pkg),
    GET_ATTR(time_window_2,pkg),
    GET_ATTR(lock,pkg),
    GET_ATTR(energy,pkg),              //PKG Energy Status
    GET_ATTR(thermal_spec_power,pkg),  //PKG Power Info
    GET_ATTR(min_power,pkg),
    GET_ATTR(max_power,pkg),
    GET_ATTR(max_time_window,pkg),
    NULL, //Perf Status only on servers! Created dynamically, Entry 14
    NULL
};

static struct attribute *pp0_attrs[] = {
    GET_ATTR(limit,pp0),           //PP0 Power Limit
    GET_ATTR(enable_limit,pp0),
    GET_ATTR(clamping_limit,pp0),
    GET_ATTR(time_window,pp0),
    GET_ATTR(lock,pp0),
    GET_ATTR(energy,pp0),          //PP0 Energy Status
    NULL, //Priority only on non-servers, Entry 6
    NULL, //Perf Status only on non-servers, Entry 7
    NULL
};

static struct attribute *pp1_attrs[] = {   //only on non-servers
    GET_ATTR(limit,pp1),           //PP1 Power Limit
    GET_ATTR(enable_limit,pp1),
    GET_ATTR(clamping_limit,pp1),
    GET_ATTR(time_window,pp1),
    GET_ATTR(lock,pp1),
    GET_ATTR(energy,pp1),          //PP1 Energy Status
    GET_ATTR(priority,pp1),          //PP1 Policy
    NULL
};

static struct attribute *dram_attrs[] = {  //only on servers
    GET_ATTR(limit,dram),          //DRAM Power Limit
    GET_ATTR(enable_limit,dram),
    GET_ATTR(clamping_limit,dram),
    GET_ATTR(time_window,dram),
    GET_ATTR(lock,dram),
    GET_ATTR(energy,dram),         //DRAM Energy Status
    GET_ATTR(throttle_time,dram),  //DRAM Perf Status
    GET_ATTR(thermal_spec_power,dram),  //DRAM Power Info
    GET_ATTR(min_power,dram),
    GET_ATTR(max_power,dram),
    GET_ATTR(max_time_window,dram),
};

static struct attribute *attrs[] = {
    GET_ATTR(energy_unit,units),
    GET_ATTR(time_unit,units),
    GET_ATTR(power_unit,units),
    NULL
};

static struct attribute_group pkg_group   = { .attrs = pkg_attrs };
static struct attribute_group krapl_group = { .attrs = attrs };
static struct attribute_group dram_group  = { .attrs = dram_attrs };
static struct attribute_group pp1_group   = { .attrs = pp1_attrs };
static struct attribute_group pp0_group   = { .attrs = pp0_attrs };

static void krapl_cleanup(void) {
    int i, node;
    for_each_node(node) { 
        for (i=0; i < 5; i++) 
            kobject_put(krapl_kobject[node][i]);
    }
}

static int __init krapl_init(void)
{
    unsigned int eax, ebx, ecx, edx, family, model;
    int node;
    bool isServer = false;
    char buf[32];
    cpuid(0,&eax,&ebx,&ecx,&edx);
    failout(ebx != 0x756e6547, "Only Intel CPUs are supported!\n",-ENODEV);
    cpuid(1,&eax,&ebx,&ecx,&edx);
    family = ((eax>>8)&0xF) + ((eax>>20)&0xFF);
    model = ((eax>>12)&0xF0) | ((eax>>4)&0xF);
    failout(family != 0x06 || model < SANDY,"Only Sandy Bridge and newer CPUs supported!\n",-ENODEV);

    //Detecting RAPL capabilities
    switch (model) {
        case SANDY: //0x2A
        case IVY: //0x3A
        case HASWELL: //0x3C
            isServer = false;
            //Exclusive
            break;
        case SANDY_EP: //0x2D
        case IVY_E: //0x3E
            isServer = true;
            break;
    }

    //Done.
    failout((node=subsys_system_register(&rapl_subsys,NULL)),"Unable to register bus!\n",node);
    #define MAKE_NODE(type,name,parent) \
        failout(!(krapl_kobject[node][type] = kobject_create_and_add(name,parent)),\
                "Could not create entry in node\n", -ENOMEM)
    #define MAKE_GROUP(type, group) \
        do { if (sysfs_create_group(krapl_kobject[node][type], group)) { \
            krapl_cleanup(); \
            return -ENOMEM; }} while (0)
    for_each_node(node) {
        if (!sprintf(buf,"rapl%u",node)) return -ENOMEM;
        MAKE_NODE(SYS_ROOT,buf,&rapl_subsys.dev_root->kobj);
        MAKE_NODE(SYS_PKG,"PKG",krapl_kobject[node][SYS_ROOT]);
        MAKE_NODE(SYS_PP0,"PP0",krapl_kobject[node][SYS_ROOT]);
        if (isServer) {
            pkg_attrs[14] = GET_ATTR(throttle_time,pkg);
            MAKE_NODE(SYS_DRAM, "DRAM", krapl_kobject[node][SYS_ROOT]);
            if (sysfs_create_group(krapl_kobject[node][SYS_DRAM], &dram_group)) {
                krapl_cleanup();
                return -ENOMEM;
            }
        } else {
            pp0_attrs[6] = GET_ATTR(priority,pp0);
            pp0_attrs[7] = GET_ATTR(throttle_time,pp0);
            MAKE_NODE(SYS_PP1, "PP1", krapl_kobject[node][SYS_ROOT]);
            MAKE_GROUP(SYS_PP1,&pp1_group);
        }

        MAKE_GROUP(SYS_ROOT, &krapl_group);
        MAKE_GROUP(SYS_PKG, &pkg_group);
        MAKE_GROUP(SYS_PP0, &pp0_group);
    }
    #undef MAKE_NODE
    #undef MAKE_GROUP

    printk(KERN_INFO "Krapl loaded!\n");
    return 0;
}

static void __exit krapl_exit(void) 
{
    put_device(rapl_subsys.dev_root);
    bus_unregister(&rapl_subsys);
    krapl_cleanup();
    printk(KERN_INFO "Krapl unloaded!\n");
}

MODULE_AUTHOR("Marcus Haehnel <mhaehnel@tudos.org>");
MODULE_DESCRIPTION("An INTEL RAPL Driver");
MODULE_LICENSE("GPL");

module_init(krapl_init);
module_exit(krapl_exit)

/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _X6837_OF_COMPAT_H
#define _X6837_OF_COMPAT_H

#include <linux/of.h>
#include <linux/string.h>

static inline const char *x6837_of_prop(const struct device_node *np,
					const char *name, char *buf, size_t len)
{
	size_t i;

	if (!np || !name || of_find_property(np, name, NULL))
		return name;
	if (!strcmp(name, "scp-sram-size"))
		return "scp_sramSize";
	if (!strcmp(name, "mediatek,dpmaif-cap"))
		return "mediatek,dpmaif_capability";
	for (i = 0; i < len - 1 && name[i]; i++)
		buf[i] = (name[i] == '-') ? '_' : name[i];
	buf[i] = '\0';
	return of_find_property(np, buf, NULL) ? buf : name;
}

static inline int x6837_of_property_read_u32(const struct device_node *np,
					     const char *propname, u32 *out_value)
{
	char buf[96];

	return of_property_read_u32(np, x6837_of_prop(np, propname, buf, sizeof(buf)), out_value);
}

static inline int x6837_of_property_read_u32_index(const struct device_node *np,
						   const char *propname, u32 index, u32 *out_value)
{
	char buf[96];

	return of_property_read_u32_index(np, x6837_of_prop(np, propname, buf, sizeof(buf)),
					  index, out_value);
}

static inline int x6837_of_property_read_u32_array(const struct device_node *np,
						   const char *propname, u32 *out_values, size_t sz)
{
	char buf[96];

	return of_property_read_u32_array(np, x6837_of_prop(np, propname, buf, sizeof(buf)),
					  out_values, sz);
}

static inline int x6837_of_property_read_string(const struct device_node *np,
						const char *propname, const char **out_string)
{
	char buf[96];

	return of_property_read_string(np, x6837_of_prop(np, propname, buf, sizeof(buf)),
				       out_string);
}

static inline int x6837_of_property_count_u32_elems(const struct device_node *np,
						    const char *propname)
{
	char buf[96];

	return of_property_count_u32_elems(np, x6837_of_prop(np, propname, buf, sizeof(buf)));
}

static inline const void *x6837_of_get_property(const struct device_node *np,
						const char *name, int *lenp)
{
	char buf[96];

	return of_get_property(np, x6837_of_prop(np, name, buf, sizeof(buf)), lenp);
}

static inline struct device_node *x6837_of_parse_phandle(const struct device_node *np,
							 const char *phandle_name, int index)
{
	char buf[96];

	return of_parse_phandle(np, x6837_of_prop(np, phandle_name, buf, sizeof(buf)), index);
}

static inline bool x6837_of_property_read_bool(const struct device_node *np,
					       const char *propname)
{
	char buf[96];

	return of_property_read_bool(np, x6837_of_prop(np, propname, buf, sizeof(buf)));
}

#undef of_property_read_u32
#define of_property_read_u32 x6837_of_property_read_u32
#undef of_property_read_u32_index
#define of_property_read_u32_index x6837_of_property_read_u32_index
#undef of_property_read_u32_array
#define of_property_read_u32_array x6837_of_property_read_u32_array
#undef of_property_read_string
#define of_property_read_string x6837_of_property_read_string
#undef of_property_count_u32_elems
#define of_property_count_u32_elems x6837_of_property_count_u32_elems
#undef of_get_property
#define of_get_property x6837_of_get_property
#undef of_parse_phandle
#define of_parse_phandle x6837_of_parse_phandle
#undef of_property_read_bool
#define of_property_read_bool x6837_of_property_read_bool

#endif /* _X6837_OF_COMPAT_H */

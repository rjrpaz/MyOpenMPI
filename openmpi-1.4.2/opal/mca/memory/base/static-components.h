/*
 * $HEADER$
 */
#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

extern const mca_base_component_t mca_memory_ptmalloc2_component;

const mca_base_component_t *mca_memory_base_static_components[] = {
  &mca_memory_ptmalloc2_component, 
  NULL
};

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif


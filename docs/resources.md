# Resources

World tiles may contain finite iron or copper deposits. Resource names are
inspected safely with `factory_resource_name`; unknown enum values report
`invalid resource`.

Extractor placement records the supported deposit type and its output mapping:

```text
iron resource   → iron ore
copper resource → copper ore
```

The extractor verifies that the underlying resource type still matches before
producing. Empty and unknown deposits cannot receive an extractor.

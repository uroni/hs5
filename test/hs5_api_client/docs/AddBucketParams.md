# AddBucketParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**bucket_name** | **str** |  | 

## Example

```python
from hs5_api.models.add_bucket_params import AddBucketParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddBucketParams from a JSON string
add_bucket_params_instance = AddBucketParams.from_json(json)
# print the JSON string representation of the object
print(AddBucketParams.to_json())

# convert the object into a dict
add_bucket_params_dict = add_bucket_params_instance.to_dict()
# create an instance of AddBucketParams from a dict
add_bucket_params_from_dict = AddBucketParams.from_dict(add_bucket_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)



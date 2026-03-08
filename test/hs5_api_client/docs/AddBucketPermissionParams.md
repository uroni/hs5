# AddBucketPermissionParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**user_id** | **str** |  | 
**bucket_name** | **str** |  | 
**add_bucket_permissions** | **List[str]** |  | 

## Example

```python
from hs5_api.models.add_bucket_permission_params import AddBucketPermissionParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddBucketPermissionParams from a JSON string
add_bucket_permission_params_instance = AddBucketPermissionParams.from_json(json)
# print the JSON string representation of the object
print(AddBucketPermissionParams.to_json())

# convert the object into a dict
add_bucket_permission_params_dict = add_bucket_permission_params_instance.to_dict()
# create an instance of AddBucketPermissionParams from a dict
add_bucket_permission_params_from_dict = AddBucketPermissionParams.from_dict(add_bucket_permission_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)



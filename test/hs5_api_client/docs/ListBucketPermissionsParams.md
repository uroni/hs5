# ListBucketPermissionsParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**bucket_name** | **str** |  | 

## Example

```python
from hs5_api.models.list_bucket_permissions_params import ListBucketPermissionsParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListBucketPermissionsParams from a JSON string
list_bucket_permissions_params_instance = ListBucketPermissionsParams.from_json(json)
# print the JSON string representation of the object
print(ListBucketPermissionsParams.to_json())

# convert the object into a dict
list_bucket_permissions_params_dict = list_bucket_permissions_params_instance.to_dict()
# create an instance of ListBucketPermissionsParams from a dict
list_bucket_permissions_params_from_dict = ListBucketPermissionsParams.from_dict(list_bucket_permissions_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)



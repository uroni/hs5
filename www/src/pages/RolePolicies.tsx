import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Dropdown, Field, makeStyles, Option, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { useParams } from 'react-router-dom';
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListRolePolicies, postApiV1B64Be5124B034028A58913931942E205AddRolePolicy, postApiV1B64Be5124B034028A58913931942E205RemoveRolePolicy, postApiV1B64Be5124B034028A58913931942E205ListPolicies } from '../api';
import { state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type RolePolicyType = {
  id: string;
  policyId: string;
  policyName: string;
  system?: boolean;
};

type PolicyType = {
  id: string;
  name: string;
  document: string;
};

function filterRolePolicies(item: RolePolicyType, search: string) {
  const searchableFields = {
    policyName: item.policyName,
  };

  return filterBySearch(search, searchableFields);
}

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
  formRow: {
    display: "flex",
    gap: tokens.spacingHorizontalM,
    alignItems: "flex-end",
    marginBottom: tokens.spacingVerticalM,
  },
});

const RolePolicies = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();
  const { roleId, roleName } = useParams<{ roleId: string; roleName: string }>();

  const snap = useSnapshot(state);

  const rolePoliciesResult = useSuspenseQuery({
    queryKey: ["rolePolicies", snap.session, roleId],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListRolePolicies({requestBody: {ses: snap.session, roleId: roleId!}}); },
  });

  const policiesResult = useSuspenseQuery({
    queryKey: ["policies", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListPolicies({requestBody: {ses: snap.session}}); },
  });

  const data = rolePoliciesResult.data!.rolePolicies as RolePolicyType[];
  const allPolicies = policiesResult.data!.policies as PolicyType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<RolePolicyType>(
    sortedData,
    filterRolePolicies,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Add policy state
  const [selectedPolicyId, setSelectedPolicyId] = useState<string>('');
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');

  // Remove policy state
  const [isRemoving, setIsRemoving] = useState<string | null>(null);
  const [removeDialogOpen, setRemoveDialogOpen] = useState(false);
  const [policyToRemove, setPolicyToRemove] = useState<RolePolicyType | null>(null);

  const handleAddPolicy = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!selectedPolicyId) return;
    
    setIsAdding(true);
    setAddError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205AddRolePolicy({
        requestBody: { ses: snap.session, roleId: roleId!, policyId: selectedPolicyId }
      });
      setSelectedPolicyId('');
      await queryClient.invalidateQueries({ queryKey: ["rolePolicies", snap.session, roleId] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to add policy');
      } else {
        setAddError('Failed to add policy');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleRemovePolicy = async (rolePolicy: RolePolicyType) => {
    setIsRemoving(rolePolicy.id);
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemoveRolePolicy({
        requestBody: { ses: snap.session, id: rolePolicy.id }
      });
      await queryClient.invalidateQueries({ queryKey: ["rolePolicies", snap.session, roleId] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to remove policy:', e.msg);
      }
    } finally {
      setIsRemoving(null);
      setRemoveDialogOpen(false);
      setPolicyToRemove(null);
    }
  };

  const columns: TableColumnDefinition<RolePolicyType>[] = [
    createTableColumn<RolePolicyType>({
      columnId: "policyName",
      renderHeaderCell: () => {
        return "Policy Name";
      },
      compare: (a, b) => {
        return a.policyName.localeCompare(b.policyName);
      },
      renderCell: (item) => {
        return <TableCellLayout>{item.policyName}</TableCellLayout>;
      },
    }),
    createTableColumn<RolePolicyType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<DeleteRegular />}
              disabled={isRemoving === item.id || item.system}
              onClick={(e) => {
                e.stopPropagation();
                setPolicyToRemove(item);
                setRemoveDialogOpen(true);
              }}
            >
              {isRemoving === item.id ? 'Removing...' : 'Remove'}
            </Button>
          </TableCellLayout>
        );
      }
    }),
  ];

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Policies for role: {roleName}</h3>
        
        {/* Add Policy Form */}
        <form onSubmit={handleAddPolicy} className={classes.formRow}>
          <Field label="Add Policy" validationMessage={addError} validationState={addError ? "error" : "none"}>
            <Dropdown
              placeholder="Select a policy"
              value={allPolicies.find(p => p.id === selectedPolicyId)?.name || ''}
              onOptionSelect={(_, data) => setSelectedPolicyId(data.optionValue || '')}
              disabled={isAdding}
            >
              {allPolicies.map((policy) => (
                <Option key={policy.id} value={policy.id}>
                  {policy.name}
                </Option>
              ))}
            </Dropdown>
          </Field>
          <Button type="submit" icon={<AddRegular />} appearance="primary" disabled={isAdding || !selectedPolicyId}>
            {isAdding ? 'Adding...' : 'Add Policy'}
          </Button>
        </form>

        <div className="cluster">
          <SearchBox onSearch={setSearch} />
          <PaginationItemsPerPageSelector
            itemsPerPage={itemsPerPage}
            setItemsPerPage={setItemsPerPage}
          />
          <DataGrid
              key={`${sortState?.sortColumn}-${sortState?.sortDirection}`}
              sortable
              onSortChange={onSortChange}
              items={pageData[page]}
              getRowId={(item) => item.id}
              columns={columns}
            >
              <DataGridHeader>
                <DataGridRow>
                  {({ renderHeaderCell, columnId, compare }) => (
                    <DataGridHeaderCell
                      sortDirection={
                        sortState?.sortColumn === columnId
                          ? sortState.sortDirection
                          : undefined
                      }
                      button={{
                        onClick: () => {
                          const newSortState: typeof sortState = {
                            sortColumn: columnId,
                            sortDirection:
                              sortState?.sortDirection === "descending" ||
                              sortState?.sortColumn !== columnId
                                ? "ascending"
                                : "descending",
                          };

                          if (newSortState.sortDirection === "ascending") {
                            const newSortedData = sortedData.sort(compare);
                            setSortedData(newSortedData);
                          }

                          if (newSortState.sortDirection === "descending") {
                            const newSortedData = sortedData.sort((a, b) =>
                              compare(b, a),
                            );
                            setSortedData(newSortedData);
                          }

                          setSortState(newSortState);
                        },
                      }}
                    >
                      {renderHeaderCell()}
                    </DataGridHeaderCell>
                  )}
                </DataGridRow>
              </DataGridHeader>
              <DataGridBody<RolePolicyType>>
                {({ item, rowId }) => (
                  <DataGridRow<RolePolicyType>
                    key={rowId}
                  >
                    {({ renderCell }) => (
                      <DataGridCell>{renderCell(item)}</DataGridCell>
                    )}
                  </DataGridRow>
                )}
              </DataGridBody>
            </DataGrid>
            <Pagination
              pageCount={pageData.length}
              page={page}
              itemsPerPage={itemsPerPage}
              totalItemCount={filteredItems.length}
              setPage={setPage}
            />
        </div>
      </TableWrapper>

      {/* Remove Confirmation Dialog */}
      <Dialog open={removeDialogOpen} onOpenChange={(_, data) => setRemoveDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Remove Policy</DialogTitle>
            <DialogContent>
              Are you sure you want to remove the policy "{policyToRemove?.policyName}" from this role?
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => policyToRemove && handleRemovePolicy(policyToRemove)}
                disabled={isRemoving !== null}
              >
                {isRemoving ? 'Removing...' : 'Remove'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default RolePolicies;
